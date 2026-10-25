/* GPL-2.0-or-later. Owned, single-slot cooperative worker handoff.
 * No native permission is conferred by this transport. Native seam admission
 * must precede taking a request; Lua and guest execution never hold this mutex.
 */
#ifndef DOSBOX_LUA_DRIVER_HANDOFF_H
#define DOSBOX_LUA_DRIVER_HANDOFF_H
#include <condition_variable>
#include <mutex>
#include <utility>
#include "native_debug_execution.h"
#include <string>
#include <vector>
namespace LuaDriver {
enum class Operation { TextSnapshot, Context, Registers, Read, Step, Run, RunUntil, Over, Save, StateReady,
                       Breakpoint, Watchpoint, Remove };
struct Request {
    std::vector<NativeDebug::StopCondition> stop_any;
    NativeDebug::TextRegion region;
    Operation operation=Operation::Context;
    NativeDebug::Context context;
    uint32_t address=0, length=0, timeout_ms=30000;
    uint16_t cs=0, ip=0;
    // Watchpoint uses cs:address (32-bit offset), breakpoint cs:ip (16-bit).
    // Native may precisely refuse unsupported memory instrumentation (e.g.
    // non-heavy builds); an owned observer ID is never a Lua callback pointer.
    uint64_t registration=0;
    bool observer=false;
    std::string path;
};
// Copied terminal archive facts; publication alone never means save success.
struct SaveEffects {
    bool valid=false, completed=false, published=false, durable=false;
    bool cleanup_attempted=false, cleanup_failed=false, poisoned=false;
    bool retired=false, usable=false;
    uint64_t archive_bytes=0;
    std::string archive_sha256, code;
};
struct Response {
    NativeDebug::TextSnapshot text;
    unsigned condition_index=0,match_x=0,match_y=0;
    uint64_t start_tick=0,end_tick=0,host_elapsed_ms=0;
    bool guest_advanced=false;
    std::string state_profile;
    std::vector<std::string> state_reasons;
    uint64_t registration=0;
    SaveEffects save;
    NativeDebug::Context context;
    // Full copied register image, never a guest pointer. Segment and mode
    // validity belongs to the native seam producing registers_valid.
    uint32_t eax=0, ebx=0, ecx=0, edx=0, esi=0, edi=0, ebp=0, esp=0, flags=0;
    uint16_t ds=0, es=0, fs=0, gs=0, ss=0;
    bool code_big=false;
    std::string reason, boundary, error;
    std::vector<uint8_t> bytes;
    uint32_t ip=0;
    uint16_t cs=0, ax=0, bx=0, cx=0, dx=0;
    bool registers_valid=false, continuation=false, partial=false;
};
struct Event {
    uint64_t ordinal=0, registration=0;
    Response image;
};
struct Verdict {
    bool propagate=true, violation=false, error=false;
};
// One producer (Lua worker), one consumer (native thread). Closing does not
// erase a response: completed effects must be consumed before close is reported.
class Handoff {
    enum class Phase { Starting, Held, Request, Executing, Event,
                       Observing, ReadRequest, Reading, ReadResult, Verdict, Result };
    Phase phase_=Phase::Starting;
    std::mutex mutex_;
    std::condition_variable changed_;
    Request request_;
    Response response_;
    Event event_;
    Verdict verdict_;
    Request read_;
    Response read_response_;
    bool closing_=false, lua_closed_=false;
public:
    Handoff()=default;
    Handoff(const Handoff &)=delete;
    Handoff &operator=(const Handoff &)=delete;
    bool ready() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(closing_ || phase_!=Phase::Starting)return false;
        phase_=Phase::Held;changed_.notify_all();return true;
    }
    bool waitReady() {
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait(lock,[this]{return closing_ || phase_!=Phase::Starting;});
        return !closing_;
    }
    template<class Observer>
    bool call(Request request,Response &response,Observer observer) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(closing_ || phase_!=Phase::Held) {
            if(phase_==Phase::Observing)verdict_.violation=true;
            return false;
        }
        request_=std::move(request);phase_=Phase::Request;changed_.notify_all();
        for (;;) {
            changed_.wait(lock,[this]{return phase_==Phase::Result || phase_==Phase::Event || closing_;});
            if(phase_!=Phase::Event)break;
            // Native cannot replace/expire this owned image until Verdict is
            // published below. Borrow it through the callback rather than copy
            // allocating after the native operation has started waiting.
            const Event &event=event_;phase_=Phase::Observing;
            Verdict verdict;
            const bool invoke=!closing_;
            lock.unlock();
            if(invoke) {
                try {verdict=observer(event);} catch (...) {verdict.error=true;}
            } else verdict.error=true;
            lock.lock();
            verdict.violation=verdict.violation || verdict_.violation;
            verdict.error=verdict.error || closing_;
            if(verdict.violation || verdict.error)verdict.propagate=true;
            verdict_=verdict;phase_=Phase::Verdict;changed_.notify_all();
        }
        // A claimed operation must settle even during close. Neither an event
        // nor an executing native effect may be abandoned by its Lua waiter.
        changed_.wait(lock,[this]{return phase_==Phase::Result || phase_==Phase::Held;});
        if(phase_!=Phase::Result)return false;
        response=std::move(response_);phase_=Phase::Held;
        changed_.notify_all();return true;
    }
    bool call(Request request,Response &response) {
        return call(std::move(request),response,[](const Event &){
            Verdict verdict;verdict.error=true;return verdict;
        });
    }
    bool offerEvent(Event event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(closing_ || phase_!=Phase::Executing)return false;
        event_=std::move(event);verdict_={};phase_=Phase::Event;
        changed_.notify_all();return true;
    }
    bool observerRead(uint64_t ordinal,Request read,Response &response) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(phase_!=Phase::Observing)return false;
        if(closing_ || !ordinal || ordinal!=event_.ordinal ||
           (read.operation!=Operation::Read && read.operation!=Operation::TextSnapshot) ||
           (read.operation==Operation::Read && (!read.length || read.length>4096 ||
           uint64_t(read.address)+read.length>uint64_t(UINT32_MAX)+1))) {
            verdict_.violation=true;return false;
        }
        read_=std::move(read);phase_=Phase::ReadRequest;changed_.notify_all();
        changed_.wait(lock,[this]{return phase_==Phase::ReadResult;});
        response=std::move(read_response_);phase_=Phase::Observing;
        changed_.notify_all();return true;
    }
    void observerViolation() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(phase_==Phase::Observing)verdict_.violation=true;
    }
    bool takeObserverRead(Request &read) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(phase_!=Phase::ReadRequest)return false;
        read=std::move(read_);phase_=Phase::Reading;return true;
    }
    bool completeObserverRead(Response response,bool violation=false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(phase_!=Phase::Reading)return false;
        verdict_.violation=verdict_.violation || violation;
        read_response_=std::move(response);phase_=Phase::ReadResult;
        changed_.notify_all();return true;
    }
    bool takeVerdict(Verdict &verdict) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(phase_!=Phase::Verdict)return false;
        verdict=verdict_;
        // Close may arrive after the worker has published a suppressing
        // verdict. Native consumption, not worker publication, expires the
        // event authority; closing still takes precedence at this boundary.
        verdict.error=verdict.error || closing_;
        if(verdict.error || verdict.violation)verdict.propagate=true;
        event_={};phase_=Phase::Executing;
        changed_.notify_all();return true;
    }
    bool take(Request &request) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(closing_ || phase_!=Phase::Request)return false;
        request=std::move(request_);phase_=Phase::Executing;return true;
    }
    bool complete(Response response) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(phase_!=Phase::Executing)return false;
        response_=std::move(response);phase_=Phase::Result;
        changed_.notify_all();return true;
    }
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closing_=true;
        if(phase_==Phase::Request){request_={};phase_=Phase::Held;}
        changed_.notify_all();
    }
    bool closing() {
        std::lock_guard<std::mutex> lock(mutex_);return closing_;
    }
    // Called on worker only after lua_close. The owner must poll this fact or
    // wait while servicing native settlement, and only then join without lock.
    void luaClosed() {
        std::lock_guard<std::mutex> lock(mutex_);
        lua_closed_=true;closing_=true;changed_.notify_all();
    }
    bool isLuaClosed() {
        std::lock_guard<std::mutex> lock(mutex_);return lua_closed_;
    }
};
}
#endif
