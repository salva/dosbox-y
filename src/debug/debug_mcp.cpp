#include "debug_mcp.h"

#include "config.h"

#if defined(C_DEBUG) && C_DEBUG && \
        ((defined(C_SDL_NET) && C_SDL_NET) || \
         (defined(C_SDL2_NET) && C_SDL2_NET))

#include "debug.h"
#include "native_control_owner.h"

#if defined(C_SDL2_NET) && C_SDL2_NET
#include <SDL2/SDL_net.h>
#else
#include <SDL_net.h>
#endif

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char* CONTROL_HOST = "127.0.0.1";

constexpr int SOCKET_POLL_TIMEOUT_MS = 50;
constexpr int RECONNECT_DELAY_MS = 1000;

constexpr size_t MAX_INCOMING_MESSAGES = 1024;
constexpr size_t MAX_OUTGOING_MESSAGES = 1024;
constexpr size_t MAX_CAPTURE_LINES = 16384;

// Protect against a peer continuously sending data without '\n'.
constexpr size_t MAX_RECEIVE_BUFFER = 1024 * 1024;

std::atomic<bool> g_running{false};
std::atomic<bool> g_connected{false};
// Socket loss is permanent attachment loss; only the emulator thread retires
// native work. Reconnect waits for that retirement rather than adopting it.
std::atomic<uint64_t> g_connection{0}, g_departed{0}, g_retired{0};
uint64_t g_next_connection=0; // I/O thread, never reused
uint64_t g_poll_connection=0; // emulator thread
NativeDebug::Attachment g_native_attachment; // exact current connection, emulator thread
struct Message { uint64_t connection; std::string text; };

uint16_t g_port = 0;

std::thread g_thread;

std::mutex g_incoming_mutex;
std::deque<Message> g_incoming;

std::mutex g_outgoing_mutex;
std::deque<Message> g_outgoing;

std::mutex g_capture_mutex;
bool g_capture_active = false;
size_t g_capture_discarded = 0;
std::vector<std::string> g_capture_lines;

//
// All socket operations happen ONLY in the I/O thread.
//
// This is important because SDLNet_TCP_Send/Recv/Close don't
// have to be synchronized with the DOSBox-X emulation thread.
//

TCPsocket g_socket = nullptr;
SDLNet_SocketSet g_socket_set = nullptr;

std::string g_receive_buffer;

std::string g_pending_step;
bool g_pending_resume=false;

struct ControlRequest {
    std::string id;
    std::string command;
    std::string payload;
};

bool IsSpace(const char c)
{
    return c == ' ' || c == '\t';
}

std::string TrimLeft(std::string value)
{
    while (!value.empty() && IsSpace(value.front()))
        value.erase(value.begin());

    return value;
}

std::string UpperAscii(std::string value)
{
    for (auto& c : value) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - ('a' - 'A'));
    }

    return value;
}

std::string SanitizeProtocolLine(std::string line)
{
    for (auto& c : line) {
        if (c == '\r' || c == '\n')
            c = ' ';
    }

    return line;
}

std::string FirstWordUpper(const std::string& value)
{
    const auto begin = value.find_first_not_of(" \t");
    if (begin == std::string::npos)
        return {};

    const auto end = value.find_first_of(" \t", begin);
    if (end == std::string::npos)
        return UpperAscii(value.substr(begin));

    return UpperAscii(value.substr(begin, end - begin));
}

bool IsResumeDebuggerCommand(const std::string& command)
{
    const auto word = FirstWordUpper(command);
    return word == "RUN" || word == "RUNWATCH" || word == "VRT";
}

void BeginCapture()
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);
    g_capture_lines.clear();
    g_capture_discarded = 0;
    g_capture_active = true;
}

void AbortCapture()
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);
    g_capture_active = false;
    g_capture_discarded = 0;
    g_capture_lines.clear();
}

std::vector<std::string> EndCapture()
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);
    g_capture_active = false;

    std::vector<std::string> lines;
    lines.swap(g_capture_lines);

    if (g_capture_discarded > 0) {
        char message[128];
        std::snprintf(message,
                      sizeof(message),
                      "[%lu debug output lines discarded]",
                      static_cast<unsigned long>(g_capture_discarded));
        lines.emplace_back(message);
        g_capture_discarded = 0;
    }

    return lines;
}

void CloseConnection()
{
    g_connected.store(false);
    const auto connection=g_connection.exchange(0);
    if (connection) g_departed.store(connection);

    if (g_socket) {
        if (g_socket_set)
            SDLNet_TCP_DelSocket(g_socket_set, g_socket);

        SDLNet_TCP_Close(g_socket);
        g_socket = nullptr;
    }

    if (g_socket_set) {
        SDLNet_FreeSocketSet(g_socket_set);
        g_socket_set = nullptr;
    }

    g_receive_buffer.clear();
}

bool Connect()
{
    if (g_departed.load()!=g_retired.load() || g_next_connection==UINT64_MAX)
        return false;
    IPaddress address = {};

    if (SDLNet_ResolveHost(&address, CONTROL_HOST, g_port) < 0) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_ResolveHost failed: %s\n",
                     SDLNet_GetError());

        return false;
    }

    std::fprintf(stderr,
                 "[ControlClient] connecting to %s:%u...\n",
                 CONTROL_HOST,
                 static_cast<unsigned>(g_port));

    TCPsocket socket = SDLNet_TCP_Open(&address);

    if (!socket)
        return false;

    SDLNet_SocketSet socket_set = SDLNet_AllocSocketSet(1);

    if (!socket_set) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_AllocSocketSet failed: %s\n",
                     SDLNet_GetError());

        SDLNet_TCP_Close(socket);
        return false;
    }

    if (SDLNet_TCP_AddSocket(socket_set, socket) < 0) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_TCP_AddSocket failed: %s\n",
                     SDLNet_GetError());

        SDLNet_FreeSocketSet(socket_set);
        SDLNet_TCP_Close(socket);

        return false;
    }

    g_socket = socket;
    g_socket_set = socket_set;

    g_receive_buffer.clear();

    g_connection.store(++g_next_connection);
    g_connected.store(true);

    std::fprintf(stderr,
                 "[ControlClient] connected to %s:%u\n",
                 CONTROL_HOST,
                 static_cast<unsigned>(g_port));

    return true;
}

bool SendAll(const std::string& data)
{
    if (!g_socket)
        return false;

    const char* ptr = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
        const int chunk =
                remaining > static_cast<size_t>(INT_MAX)
                        ? INT_MAX
                        : static_cast<int>(remaining);

        const int sent = SDLNet_TCP_Send(
                g_socket,
                ptr,
                chunk);

        if (sent <= 0)
            return false;

        ptr += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

void QueueIncoming(std::string message)
{
    std::lock_guard<std::mutex> lock(g_incoming_mutex);

    if (g_incoming.size() >= MAX_INCOMING_MESSAGES)
        g_incoming.pop_front();

    g_incoming.push_back({g_connection.load(),std::move(message)});
}

bool PopIncoming(Message& message)
{
    std::lock_guard<std::mutex> lock(g_incoming_mutex);

    if (g_incoming.empty())
        return false;

    message = std::move(g_incoming.front());
    g_incoming.pop_front();
    return true;
}

void ProcessReceiveBuffer()
{
    for (;;) {
        const auto newline = g_receive_buffer.find('\n');

        if (newline == std::string::npos)
            break;

        std::string line =
                g_receive_buffer.substr(0, newline);

        g_receive_buffer.erase(0, newline + 1);

        // Support both "\n" and "\r\n".
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
            continue;

        QueueIncoming(std::move(line));
    }
}

bool Receive()
{
    char buffer[8192];

    const int received =
            SDLNet_TCP_Recv(
                    g_socket,
                    buffer,
                    sizeof(buffer));

    if (received <= 0)
        return false;

    g_receive_buffer.append(
            buffer,
            static_cast<size_t>(received));

    if (g_receive_buffer.size() > MAX_RECEIVE_BUFFER) {
        std::fprintf(stderr,
                     "[ControlClient] receive buffer overflow\n");

        return false;
    }

    ProcessReceiveBuffer();

    return true;
}

bool FlushOutgoing()
{
    for (;;) {
        Message message;
        {
            std::lock_guard<std::mutex> lock(g_outgoing_mutex);
            if (g_outgoing.empty()) return true;
            message=std::move(g_outgoing.front());
            g_outgoing.pop_front();
        }
        if (message.connection!=g_connection.load()) continue;
        // Failed sends may have partial effects; never replay on reconnect.
        if (!SendAll(message.text)) return false;
    }
}

void SleepReconnectDelay()
{
    //
    // Do it in short steps so ControlServer_Stop()
    // doesn't have to wait a full second.
    //

    constexpr int step_ms = 50;

    for (int elapsed = 0;
         elapsed < RECONNECT_DELAY_MS && g_running.load();
         elapsed += step_ms) {

        std::this_thread::sleep_for(
                std::chrono::milliseconds(step_ms));
    }
}

void ThreadMain()
{
    while (g_running.load()) {

        //
        // Connect / reconnect
        //

        if (!g_socket) {
            if (!Connect()) {
                SleepReconnectDelay();
                continue;
            }
        }

        //
        // Send everything currently queued.
        //

        if (!FlushOutgoing()) {
            std::fprintf(stderr,
                         "[ControlClient] connection lost while sending\n");

            CloseConnection();
            continue;
        }

        //
        // Wait for incoming data.
        //

        const int ready =
                SDLNet_CheckSockets(
                        g_socket_set,
                        SOCKET_POLL_TIMEOUT_MS);

        if (ready < 0) {
            std::fprintf(stderr,
                         "[ControlClient] SDLNet_CheckSockets failed: %s\n",
                         SDLNet_GetError());

            CloseConnection();
            continue;
        }

        if (ready == 0)
            continue;

        if (g_socket && SDLNet_SocketReady(g_socket)) {
            if (!Receive()) {
                std::fprintf(stderr,
                             "[ControlClient] connection closed\n");

                CloseConnection();
                continue;
            }
        }
    }

    CloseConnection();
}

bool ParseControlRequest(const std::string& line,
                         ControlRequest& request,
                         std::string& error)
{
    if (line.compare(0, 4, "REQ ") != 0) {
        error = "expected REQ <id> <PING|BREAK|EXEC>";
        return false;
    }

    const auto id_begin = line.find_first_not_of(" \t", 4);
    if (id_begin == std::string::npos) {
        error = "missing request id";
        return false;
    }

    const auto id_end = line.find_first_of(" \t", id_begin);
    if (id_end == std::string::npos) {
        error = "missing request command";
        return false;
    }

    request.id = line.substr(id_begin, id_end - id_begin);

    const auto command_begin = line.find_first_not_of(" \t", id_end);
    if (command_begin == std::string::npos) {
        error = "missing request command";
        return false;
    }

    const auto command_end = line.find_first_of(" \t", command_begin);
    if (command_end == std::string::npos) {
        request.command = UpperAscii(line.substr(command_begin));
        request.payload.clear();
    } else {
        request.command = UpperAscii(
                line.substr(command_begin, command_end - command_begin));
        request.payload = TrimLeft(line.substr(command_end));
    }

    if (request.command != "PING" &&
            request.command != "BREAK" &&
            request.command != "EXEC") {
        error = "unknown request command";
        return false;
    }

    if (request.command == "EXEC" && request.payload.empty()) {
        error = "missing debugger command";
        return false;
    }

    return true;
}

void SendResponse(const std::string& id,
                  const bool ok,
                  const std::vector<std::string>& lines)
{
    std::string response = std::string("BEGIN ") + id + (ok ? " OK" : " ERR");

    for (const auto& line : lines) {
        response.push_back('\n');
        response += SanitizeProtocolLine(line);
    }

    response += "\nEND ";
    response += id;

    ControlServer_Send(std::move(response));
}

void SendErrorResponse(const std::string& id, const std::string& error)
{
    SendResponse(id.empty() ? "0" : id, false, {error});
}

void ProcessControlCommand(const std::string& line)
{
    ControlRequest request;
    std::string error;

    if (!ParseControlRequest(line, request, error)) {
        SendErrorResponse(request.id, error);
        return;
    }

    if (request.command == "PING") {
        SendResponse(request.id, true, {"PONG"});
        return;
    }

    if (!NativeControl::rawEffectAllowed() || DEBUG_ContinuationUnavailable()) {
        SendErrorResponse(request.id, "native control unavailable or owned");
        return;
    }

    // Interrupt delivery must not fall through to Interactive ownership.
    if (request.command=="EXEC" &&
        (FirstWordUpper(request.payload)=="INT" || FirstWordUpper(request.payload)=="INTT")) {
        uint8_t value=0;
        const auto begin=request.payload.find_first_not_of(" \t");
        const auto end=request.payload.find_first_of(" \t",begin);
        NativeDebug::Request interrupt;
        interrupt.frontend=NativeDebug::Frontend::Bridge;
        interrupt.attachment=g_native_attachment;
        interrupt.expected=DEBUG_NativeExecutionContext();
        if (!DEBUG_AdmitNativeEffect(interrupt.attachment)) {
            SendErrorResponse(request.id,"native execution admission refused");return;
        }
        // Expressions may read registers: admit the held context first.
        if (!DEBUG_ParseInterruptArgument(end==std::string::npos ? "" : request.payload.c_str()+end,value)) {
            SendErrorResponse(request.id,"invalid interrupt expression");return;
        }
        interrupt.resume=FirstWordUpper(request.payload)=="INT";
        interrupt.action=interrupt.resume ? NativeDebug::Request::Action::RunInterrupt :
                                          NativeDebug::Request::Action::TraceInterrupt;
        interrupt.interrupt=static_cast<uint8_t>(value);
        const bool accepted=DEBUG_RequestNativeExecution(interrupt)==NativeDebug::Admission::Accepted;
        g_pending_resume=accepted;
        SendResponse(request.id,accepted,{accepted ? "OK" : "native execution admission refused"});
        return;
    }

    // Match the debugger parser's command-token boundary. Trailing spaces
    // must not fall through and acquire an Interactive-owned step result.
    if (request.command=="EXEC" && FirstWordUpper(request.payload)=="STEPIN") {
        if (!g_pending_step.empty()) {
            SendErrorResponse(request.id,"native step pending");return;
        }
        NativeDebug::Request step;
        step.frontend=NativeDebug::Frontend::Bridge;
        step.attachment=g_native_attachment;
        step.expected=DEBUG_NativeExecutionContext();
        if (DEBUG_RequestNativeExecution(step)!=NativeDebug::Admission::Accepted) {
            SendErrorResponse(request.id,"native step admission refused");return;
        }
        g_pending_step=request.id;
        return; // Reply only after native execution and callback unwind.
    }

    if (request.command == "EXEC" &&
            IsResumeDebuggerCommand(request.payload)) {
        // Execution admission returns before guest execution. A refused
        // request must not be acknowledged as successful.
        const auto word=FirstWordUpper(request.payload);
        const bool accepted=DEBUG_ResumeNativeExecution(
            NativeDebug::Frontend::Bridge,word=="RUNWATCH",word=="VRT",g_native_attachment);
        g_pending_resume=accepted;
        SendResponse(request.id,accepted,
                     {accepted ? "OK" : "native execution admission refused"});
        return;
    }

#if C_HEAVY_DEBUG
    // Logging is execution plus owned instrumentation, not a text-parser effect.
    if (request.command=="EXEC") {
        const auto word=FirstWordUpper(request.payload);
        if (word=="LOG" || word=="LOGS" || word=="LOGL" || word=="LOGC") {
            NativeDebug::Request log;
            log.frontend=NativeDebug::Frontend::Bridge;
            log.attachment=g_native_attachment;
            log.expected=DEBUG_NativeExecutionContext();
            if (!DEBUG_AdmitNativeEffect(log.attachment)) {
                SendErrorResponse(request.id,"native execution admission refused");return;
            }
            const auto begin=request.payload.find_first_not_of(" \t");
            const auto end=request.payload.find_first_of(" \t",begin);
            DEBUG_ParseLogArgument(end==std::string::npos ? "" : request.payload.c_str()+end,log.log_count);
            log.resume=true;log.log=true;
            log.log_type=word=="LOG" ? 1 : word=="LOGL" ? 2 : word=="LOGC" ? 3 : 0;
            const bool accepted=DEBUG_RequestNativeExecution(log)==NativeDebug::Admission::Accepted;
            g_pending_resume=accepted;
            SendResponse(request.id,accepted,{accepted ? "OK" : "native execution admission refused"});
            return;
        }
    }
#endif

    BeginCapture();

    bool ok = true;
    try {
        if (request.command == "BREAK") {
            DEBUG_EnableDebugger();
        } else {
            ok = DEBUG_ExecuteSessionCommand(request.payload.c_str(),g_native_attachment);
        }
    } catch (...) {
        // Effects may already have occurred. Retire capture, never replay.
        AbortCapture();
        throw;
    }

    auto output = EndCapture();
    SendResponse(request.id, ok, output);
}

} // namespace

void ControlServer_Start(const uint16_t port)
{
    if (port == 0)
        return;

    if (g_running.load())
        return;

    if (SDLNet_Init() < 0) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_Init failed: %s\n",
                     SDLNet_GetError());

        return;
    }

    g_port = port;

    {
        std::lock_guard<std::mutex> lock(g_incoming_mutex);
        g_incoming.clear();
    }

    {
        std::lock_guard<std::mutex> lock(g_outgoing_mutex);
        g_outgoing.clear();
    }

    g_running.store(true);

    g_thread = std::thread(ThreadMain);
}

void ControlServer_StopForFatalExit()
{
    if (!g_running.exchange(false))
        return;

    if (g_thread.joinable())
        g_thread.join();

    g_connected.store(false);
    g_port = 0;

    {
        std::lock_guard<std::mutex> lock(g_incoming_mutex);
        g_incoming.clear();
    }

    {
        std::lock_guard<std::mutex> lock(g_outgoing_mutex);
        g_outgoing.clear();
    }

    EndCapture();
}

void ControlServer_Stop()
{
    ControlServer_StopForFatalExit();
    if ((!g_native_attachment.serial || DEBUG_DetachNativeExecution(g_native_attachment))) {
        g_pending_step.clear();g_pending_resume=false;g_native_attachment={};
        g_poll_connection=0;
        g_retired.store(g_departed.load());
    }
}

bool ControlServer_IsConnected()
{
    return g_connected.load();
}

void ControlServer_Send(std::string message)
{
    const auto connection=g_connection.load();
    if (!g_running.load() || !connection || connection!=g_poll_connection)
        return;

    message.push_back('\n');

    std::lock_guard<std::mutex> lock(g_outgoing_mutex);

    if (g_outgoing.size() >= MAX_OUTGOING_MESSAGES)
        g_outgoing.pop_front();

    g_outgoing.push_back({connection,std::move(message)});
}

void ControlServer_SendEvent(
        const std::string& event,
        const std::string& data)
{
    ControlServer_Send(std::string("BEGIN event OK\n") +
                       SanitizeProtocolLine(event) + " " +
                       SanitizeProtocolLine(data) +
                       "\nEND event");
}

void ControlServer_RetireDeparture()
{
    const auto departed=g_departed.load();
    if (departed!=g_retired.load()) {
        if (!(!g_native_attachment.serial || DEBUG_DetachNativeExecution(g_native_attachment))) return;
        g_pending_step.clear();g_pending_resume=false;g_native_attachment={};g_poll_connection=0;
        {
            std::lock_guard<std::mutex> lock(g_incoming_mutex);
            g_incoming.clear();
        }
        {
            std::lock_guard<std::mutex> lock(g_outgoing_mutex);
            g_outgoing.clear();
        }
        g_retired.store(departed);
    }
}

void ControlServer_Poll()
{
    // Pending work can retire without execution. Active detach refuses until
    // the callback owner unwinds. Neither state permits command dispatch.
    ControlServer_RetireDeparture();
    if (DEBUG_NativeExecutionInFlight()) {
        // Continuing grants still accept an explicit stop. Never parse an EXEC
        // mutation or a second execution request on that active grant.
        if (DEBUG_NativeExecutionPending()) return;
        g_poll_connection=g_connection.load();
        Message command;
        while (PopIncoming(command)) {
            if (!g_poll_connection || command.connection!=g_poll_connection ||
                command.connection!=g_connection.load()) continue;
            ControlRequest request;std::string error;
            if (!ParseControlRequest(command.text,request,error))
                SendErrorResponse(request.id,error);
            else if (request.command=="BREAK" || request.command=="PING")
                ProcessControlCommand(command.text);
            else SendErrorResponse(request.id,"native execution active");
        }
        return;
    }
    g_poll_connection=g_connection.load();
    if (!g_poll_connection) return;
    if (!g_native_attachment.serial) {
        g_native_attachment=DEBUG_AttachNativeExecution(NativeDebug::Frontend::Bridge,g_poll_connection);
        if (!g_native_attachment.serial) {
            Message refused;
            while (PopIncoming(refused)) {
                if (refused.connection!=g_poll_connection) continue;
                ControlRequest request;std::string error;
                ParseControlRequest(refused.text,request,error);
                SendErrorResponse(request.id,"debugger session already attached");
            }
            return;
        }
    }
    if (g_pending_resume) {
        const auto *result=DEBUG_NativeExecutionResult(g_native_attachment);
        if (!result) return;
        const auto completed=*result;
        if (!DEBUG_ReleaseNativeExecutionResult(g_native_attachment,completed.operation))
            return;
        g_pending_resume=false;
        // RUN/INT/LOG acknowledge admission, not successful execution. Report
        // the eventual owned terminal via the existing event channel. Retire
        // before sending: uncertain transport delivery must never replay work
        // or strand a connected terminal slot.
        char line[256];
        std::snprintf(line,sizeof(line),
            "operation=%llu failed=%u budget_exhausted=%u decoder_calls=%u CS:IP=%04X:%08X",
            static_cast<unsigned long long>(completed.operation),
            completed.failed ? 1u : 0u,completed.budget_exhausted ? 1u : 0u,
            completed.decoder_calls,completed.cs,completed.ip);
        ControlServer_SendEvent("execution_terminal",line);
    }
    if (!g_pending_step.empty()) {
        const auto *result=DEBUG_NativeExecutionResult(g_native_attachment);
        if (!result) return;
        // Copy before retirement invalidates the borrowed terminal. If exact
        // retirement fails, retain the pending request without repeating replies.
        const auto completed=*result;
        if (!DEBUG_ReleaseNativeExecutionResult(g_native_attachment,completed.operation))
            return;
        char line[200];
        std::snprintf(line,sizeof(line),"DEBUG: STEPIN CS:IP=%04X:%04X AX=%04X BX=%04X CX=%04X DX=%04X",
            completed.cs,completed.ip,completed.ax,completed.bx,completed.cx,completed.dx);
        SendResponse(g_pending_step,!completed.failed,{line});
        g_pending_step.clear();g_pending_resume=false;
    }
    Message command;
    while (PopIncoming(command)) {
        if (command.connection!=g_poll_connection ||
            command.connection!=g_connection.load()) continue;
        std::fprintf(stderr,
                     "[ControlClient] mcp command %s\n",
                     command.text.c_str());

        ProcessControlCommand(command.text);
        if (!g_pending_step.empty() || DEBUG_NativeExecutionPending()) return;
    }
}

bool DEBUG_MCP_IsCapturingOutput()
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);
    return g_capture_active;
}

void DEBUG_MCP_CaptureMessage(const char* message)
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);

    if (!g_capture_active)
        return;

    if (g_capture_lines.size() >= MAX_CAPTURE_LINES) {
        ++g_capture_discarded;
        return;
    }

    g_capture_lines.emplace_back(message ? message : "");
}

#else

void ControlServer_Start(uint16_t) {}
void ControlServer_Stop() {}
void ControlServer_StopForFatalExit() {}
bool ControlServer_IsConnected() { return false; }
void ControlServer_Send(std::string) {}
void ControlServer_SendEvent(const std::string&, const std::string&) {}
void ControlServer_Poll() {}
void ControlServer_RetireDeparture() {}
bool DEBUG_MCP_IsCapturingOutput() { return false; }
void DEBUG_MCP_CaptureMessage(const char*) {}

#endif
