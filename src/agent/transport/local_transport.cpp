#include "config.h"
#if defined(C_DEBUG) && defined(C_DOSBOX_AGENT)
#include "rpc_transport.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#ifdef WIN32
#include <windows.h>
#include <sddl.h>
#pragma comment(lib, "Advapi32.lib")
#endif

#ifndef WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#endif

namespace dosbox_agent {
namespace {

class NamedPipeTransport final : public IRpcTransport {
public:
    NamedPipeTransport() : impl(new Impl()) {}
    ~NamedPipeTransport() override { Stop(); }

    const char* Name() const override { return "named_pipe"; }

    bool ValidateConfiguration(const AgentConfig& config,
                               std::string* error) const override
    {
        if (config.endpoint.compare(0, 9, "\\\\.\\pipe\\") != 0 || config.endpoint.size() == 9) {
            if (error != NULL)
                *error = "Named pipe endpoint must use \\\\.\\pipe\\<name>";
            return false;
        }
        return true;
    }

    bool Start(const AgentConfig& config,
               RequestHandler handler,
               std::string* error) override
    {
        if (!ValidateConfiguration(config, error) || !handler)
            return false;

#ifndef WIN32
        if (error != NULL)
            *error = "Windows named pipes are unavailable on this platform";
        return false;
#else
        std::unique_lock<std::mutex> lock(impl->mutex);
        if (impl->listener.joinable()) {
            if (error != NULL)
                *error = "Named pipe transport is already running";
            return false;
        }

        impl->config = config;
        impl->handler = handler;
        impl->stopping = false;
        impl->ready = false;
        impl->start_succeeded = false;
        impl->start_error.clear();
        impl->listener = std::thread(&NamedPipeTransport::Run, this);
        impl->ready_condition.wait(lock, [this]() { return impl->ready; });
        if (impl->start_succeeded)
            return true;

        const std::string start_error = impl->start_error;
        lock.unlock();
        impl->listener.join();
        if (error != NULL)
            *error = start_error;
        return false;
#endif
    }

    void Stop() override
    {
#ifdef WIN32
        std::thread listener;
        std::vector<std::thread> client_workers;
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if (!impl->listener.joinable())
                return;
            impl->stopping = true;
            for (std::vector<HANDLE>::const_iterator it = impl->active_pipes.begin();
                 it != impl->active_pipes.end(); ++it)
                CancelIoEx(*it, NULL);
            listener.swap(impl->listener);
        }
        listener.join();
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            client_workers.swap(impl->client_workers);
        }
        for (std::vector<std::thread>::iterator it = client_workers.begin();
             it != client_workers.end(); ++it)
            if (it->joinable())
                it->join();
#endif
    }

private:
    class Impl {
    public:
        std::mutex mutex;
        std::condition_variable ready_condition;
        AgentConfig config;
        RequestHandler handler;
        std::thread listener;
        bool stopping = false;
        bool ready = false;
        bool start_succeeded = false;
        std::string start_error;
#ifdef WIN32
        HANDLE active_pipe = INVALID_HANDLE_VALUE;
        std::vector<HANDLE> active_pipes;
        std::vector<std::thread> client_workers;
#endif
    };

    std::unique_ptr<Impl> impl;

#ifdef WIN32
    static bool WriteAll(const HANDLE pipe, const std::string& message)
    {
        std::size_t offset = 0;
        while (offset < message.size()) {
            const DWORD remaining = static_cast<DWORD>((std::min)(message.size() - offset,
                                                                    static_cast<std::size_t>(0xffffffffu)));
            DWORD written = 0;
            if (!WriteFile(pipe, message.data() + offset, remaining, &written, NULL) || written == 0)
                return false;
            offset += written;
        }
        return true;
    }

    HANDLE CreatePipe(std::string* error)
    {
        PSECURITY_DESCRIPTOR descriptor = NULL;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:P(A;;GA;;;OW)",
                                                                    SDDL_REVISION_1,
                                                                    &descriptor,
                                                                    NULL)) {
            *error = "Unable to create named pipe security descriptor";
            return INVALID_HANDLE_VALUE;
        }

        SECURITY_ATTRIBUTES attributes;
        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = descriptor;
        attributes.bInheritHandle = FALSE;
        const DWORD buffer_size = static_cast<DWORD>((std::min)(impl->config.max_message_bytes,
                                                                 static_cast<std::size_t>(0xffffffffu)));
        HANDLE pipe = CreateNamedPipeA(impl->config.endpoint.c_str(),
                                       PIPE_ACCESS_DUPLEX,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       PIPE_UNLIMITED_INSTANCES,
                                       buffer_size,
                                       buffer_size,
                                       0,
                                       &attributes);
        LocalFree(descriptor);
        if (pipe == INVALID_HANDLE_VALUE)
            *error = "Unable to create named pipe endpoint";
        return pipe;
    }

    void RemoveActivePipe(const HANDLE pipe)
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        for (std::vector<HANDLE>::iterator it = impl->active_pipes.begin();
             it != impl->active_pipes.end(); ++it) {
            if (*it == pipe) {
                impl->active_pipes.erase(it);
                break;
            }
        }
    }

    bool ServeClient(const HANDLE pipe)
    {
        std::string pending;
        char buffer[4096];
        for (;;) {
            DWORD read = 0;
            if (!ReadFile(pipe, buffer, sizeof(buffer), &read, NULL) || read == 0)
                return false;
            pending.append(buffer, read);

            for (;;) {
                const std::string::size_type line_end = pending.find('\n');
                if (line_end == std::string::npos) {
                    if (pending.size() > impl->config.max_message_bytes) {
                        const std::string response = impl->handler(std::string(impl->config.max_message_bytes + 1, 'x')) + "\n";
                        WriteAll(pipe, response);
                        return false;
                    }
                    break;
                }

                std::string request = pending.substr(0, line_end);
                pending.erase(0, line_end + 1);
                if (!request.empty() && request[request.size() - 1] == '\r')
                    request.erase(request.size() - 1);
                if (request.size() > impl->config.max_message_bytes) {
                    const std::string response = impl->handler(std::string(impl->config.max_message_bytes + 1, 'x')) + "\n";
                    WriteAll(pipe, response);
                    return false;
                }
                if (!WriteAll(pipe, impl->handler(request) + "\n"))
                    return false;
            }

            std::lock_guard<std::mutex> lock(impl->mutex);
            if (impl->stopping)
                return false;
        }
    }

    void ServeClientThread(const HANDLE pipe)
    {
        ServeClient(pipe);
        CloseHandle(pipe);
        RemoveActivePipe(pipe);
    }

    void Run()
    {
        bool first_pipe = true;
        for (;;) {
            std::string error;
            const HANDLE pipe = CreatePipe(&error);
            if (pipe == INVALID_HANDLE_VALUE) {
                std::lock_guard<std::mutex> lock(impl->mutex);
                if (first_pipe) {
                    impl->start_error = error;
                    impl->ready = true;
                    impl->ready_condition.notify_all();
                }
                return;
            }

            {
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->active_pipe = pipe;
                impl->active_pipes.push_back(pipe);
                if (first_pipe) {
                    impl->start_succeeded = true;
                    impl->ready = true;
                    impl->ready_condition.notify_all();
                    first_pipe = false;
                }
                if (impl->stopping) {
                    CloseHandle(pipe);
                    impl->active_pipe = INVALID_HANDLE_VALUE;
                    impl->active_pipes.pop_back();
                    return;
                }
            }

            const BOOL connected = ConnectNamedPipe(pipe, NULL);
            const DWORD connect_error = connected ? ERROR_SUCCESS : GetLastError();
            if (connected || connect_error == ERROR_PIPE_CONNECTED) {
                bool stopping = false;
                {
                    std::lock_guard<std::mutex> lock(impl->mutex);
                    stopping = impl->stopping;
                    if (!stopping)
                        impl->client_workers.push_back(std::thread(&NamedPipeTransport::ServeClientThread, this, pipe));
                }
                if (stopping) {
                    CloseHandle(pipe);
                    RemoveActivePipe(pipe);
                    return;
                }
            } else {
                CloseHandle(pipe);
                RemoveActivePipe(pipe);
            }

            std::lock_guard<std::mutex> lock(impl->mutex);
            impl->active_pipe = INVALID_HANDLE_VALUE;
            if (impl->stopping)
                return;
        }
    }
#endif
};

class UnixSocketTransport final : public IRpcTransport {
public:
    const char* Name() const override { return "unix_socket"; }

    bool ValidateConfiguration(const AgentConfig& config,
                               std::string* error) const override
    {
        if (config.endpoint.empty() || config.endpoint[0] != '/') {
            if (error != NULL)
                *error = "Unix socket endpoint must be an absolute path";
            return false;
        }
        return true;
    }

    ~UnixSocketTransport() override { Stop(); }

    bool Start(const AgentConfig& config,
               RequestHandler handler,
               std::string* error) override
    {
#ifndef WIN32
        if (!ValidateConfiguration(config, error) || !handler) return false;
        if (worker.joinable()) {
            if (error) *error = "Transport is already running";
            return false;
        }
        sockaddr_un address = {};
        address.sun_family = AF_UNIX;
        if (config.endpoint.size() >= sizeof(address.sun_path)) {
            if (error) *error = "Unix socket endpoint is too long";
            return false;
        }
        const auto slash = config.endpoint.rfind('/');
        const std::string directory = slash ? config.endpoint.substr(0, slash) : "/";
        struct stat parent = {};
        if (stat(directory.c_str(), &parent) < 0 || !S_ISDIR(parent.st_mode) ||
            parent.st_uid != geteuid() || (parent.st_mode & 0077)) {
            if (error) *error = "Unix socket requires an owner-private directory";
            return false;
        }
        std::memcpy(address.sun_path, config.endpoint.c_str(), config.endpoint.size() + 1);
        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            if (error) *error = "Cannot create Unix socket";
            return false;
        }
        // Never remove an endpoint owned by an earlier/independent server.
        if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            close(fd);
            if (error) *error = "Cannot bind Unix socket (endpoint retained)";
            return false;
        }
        struct stat owned = {};
        if (lstat(config.endpoint.c_str(), &owned) < 0) {
            close(fd);
            if (error) *error = "Cannot identify bound endpoint (path retained)";
            return false;
        }
        path = config.endpoint;
        device = owned.st_dev;
        inode = owned.st_ino;
        if (chmod(path.c_str(), 0600) < 0 || listen(fd, 4) < 0) {
            close(fd);
            RemoveEndpoint();
            if (error) *error = "Cannot prepare private Unix socket";
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            listener = fd;
            stopping = false;
        }
        try {
            worker = std::thread([this, config, handler, fd]() {
                Serve(config, handler, fd);
            });
        } catch (...) {
            close(fd);
            listener = -1;
            RemoveEndpoint();
            if (error) *error = "Cannot start Unix socket worker";
            return false;
        }
        return true;
#else
        (void)config;
        (void)handler;
        if (error) *error = "Unix sockets are unavailable on this platform";
        return false;
#endif
    }

    void Stop() override
    {
#ifndef WIN32
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
            if (active >= 0) shutdown(active, SHUT_RDWR);
            if (listener >= 0) shutdown(listener, SHUT_RDWR);
        }
        if (worker.joinable()) worker.join();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (listener >= 0) close(listener);
            listener = -1;
        }
        RemoveEndpoint();
#endif
    }

private:
#ifndef WIN32
    void Serve(const AgentConfig& config, const RequestHandler& handler, int fd)
    {
        for (;;) {
            const int client = accept(fd, nullptr, nullptr);
            if (client < 0) {
                if (errno == EINTR) continue;
                break;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping) { close(client); break; }
                active = client;
            }
            // Handler failure may have effects. Close the connection, never replay.
            try {
                ServeClient(config, handler, client);
            } catch (...) {}
            {
                std::lock_guard<std::mutex> lock(mutex);
                active = -1;
                close(client);
                if (stopping) break;
            }
        }
    }

    void ServeClient(const AgentConfig& config, const RequestHandler& handler, int client)
    {
        std::string request;
        char buffer[4096];
        for (;;) {
            const auto count = recv(client, buffer, sizeof(buffer), 0);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) return;
            for (ssize_t n = 0; n < count; ++n) {
                if (buffer[n] == '\n') {
                    if (!request.empty() && request.back() == '\r') request.pop_back();
                    const std::string response = handler(request) + "\n";
                    request.clear();
                    std::size_t offset = 0;
                    while (offset < response.size()) {
                        const auto sent = send(client, response.data() + offset,
                                               response.size() - offset, MSG_NOSIGNAL);
                        if (sent < 0 && errno == EINTR) continue;
                        if (sent <= 0) return;
                        offset += static_cast<std::size_t>(sent);
                    }
                } else if (request.size() < config.max_message_bytes) {
                    request += buffer[n];
                } else {
                    return;
                }
            }
        }
    }

    void RemoveEndpoint()
    {
        struct stat current = {};
        if (!path.empty() && lstat(path.c_str(), &current) == 0 &&
            current.st_dev == device && current.st_ino == inode)
            unlink(path.c_str());
        path.clear();
    }
    std::mutex mutex;
    std::thread worker;
    int listener = -1, active = -1;
    bool stopping = false;
    std::string path;
    dev_t device = 0;
    ino_t inode = 0;
#endif
};

} // namespace

std::unique_ptr<IRpcTransport> AGENT_CreateLocalTransport(const AgentTransport transport)
{
    switch (transport) {
    case AgentTransport::NamedPipe:
        return std::unique_ptr<IRpcTransport>(new NamedPipeTransport());
    case AgentTransport::UnixSocket:
        return std::unique_ptr<IRpcTransport>(new UnixSocketTransport());
    }
    return std::unique_ptr<IRpcTransport>();
}

} // namespace dosbox_agent
#endif // defined(C_DEBUG) && defined(C_DOSBOX_AGENT)
