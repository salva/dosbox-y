/* GPL-2.0-or-later. Emulator-thread debugger execution ownership. */
#ifndef DOSBOX_NATIVE_DEBUG_EXECUTION_H
#define DOSBOX_NATIVE_DEBUG_EXECUTION_H
#include <cstdint>
#include "native_stop_conditions.h"
namespace NativeDebug {
enum class Frontend { Interactive, Agent, Predicate, Bridge, Lua };
enum class State { Empty, Pending, Active, Terminal };
enum class Admission { Accepted, Busy, StaleContext, UnsafeContext, Capacity };
enum class Terminal { Held, Budget, Error, LogOpenError, Exception, ContinuationLost, OwnerLost, AttachmentLost, ProgramExit };
struct Context { uint64_t invocation=0, checkpoint=0; };
struct Attachment { uint64_t serial=0; };
struct Request {
    std::vector<StopCondition> stop_any;
    // Native-boundary completion is distinct from legacy numeric budgets.
    enum class Completion { Ordinary, NativeBoundary };
    Completion completion=Completion::Ordinary;
    Frontend frontend=Frontend::Interactive;
    Context expected;
    Attachment attachment;
    uint32_t cycle_budget=1;
    uint32_t decoder_limit=100000, virtual_ms_limit=1000, wall_ms_limit=1000;
    bool retain_result=false;
    bool resume=false, over=false, watch=false, retrace=false, normal_resume=false;
    enum class Action { Execute, TraceInterrupt, RunInterrupt, Phase };
    Action action=Action::Execute;
    uint32_t phase_limit=0, phase_wall_ms=0;
    bool phase_target=false;
    uint16_t phase_cs=0, phase_ip=0;
    uint8_t interrupt=0;
    bool log=false;
    int log_type=0, log_count=0;
};
struct Result {
    StopEvidence stop;
    Terminal terminal=Terminal::Held;
    uint64_t operation=0, attachment=0;
    Frontend frontend=Frontend::Interactive;
    Context start, end;
    // Decoder invocations are not retired instructions.
    uint32_t decoder_calls=0;
    bool failed=false, budget_exhausted=false;
    uint16_t cs=0, ax=0, bx=0, cx=0, dx=0;
    uint32_t ip=0;
};
// One attachment and one retained operation. The facade validates native
// ancestry and state leases; an attachment alone is not execution permission.
class Execution {
    uint64_t serial_=0, checkpoint_=0, invocation_=0, attachment_serial_=0;
    Attachment attachment_;
    Frontend frontend_=Frontend::Interactive;
    uint64_t principal_=0;
    State state_=State::Empty;
    StopRuntime stop_;
    Request request_;
    Result result_;
    bool departing_=false, attachment_lost_=false;
    uint64_t wall_start_=0;
    double virtual_start_=0;
public:
    Attachment attach(Frontend frontend, uint64_t principal=0) {
        if (departing_) return {};
        if (attachment_.serial && (frontend_!=frontend || principal_!=principal)) return {};
        if (!attachment_.serial && attachment_serial_!=UINT64_MAX) {
            attachment_.serial=++attachment_serial_;
            frontend_=frontend;principal_=principal;
        }
        return attachment_;
    }
    bool admits(Attachment attachment) const {
        return !departing_ && attachment.serial && attachment.serial==attachment_.serial;
    }
    bool close(Attachment attachment) {
        if (!attachment.serial || attachment.serial!=attachment_.serial) return false;
        departing_=true;
        if (state_==State::Active || state_==State::Pending) attachment_lost_=true;
        return true;
    }
    bool detach(Attachment attachment) {
        if (!attachment.serial || attachment.serial!=attachment_.serial) return false;
        departing_=true;
        if (state_==State::Active) {attachment_lost_=true;return false;}
        if (state_==State::Pending) settle(0,true);
        state_=State::Empty;request_={};result_={};stop_={};
        attachment_={};departing_=false;return true;
    }
    bool ownerDeparting(uint64_t invocation) {
        if (!invocation || invocation!=invocation_) return false;
        // A local active grant owns a retained continuation, not the lifetime
        // of its start token. Native restoration/lifecycle frontiers settle it.
        // Pending local requests have not acquired that continuation yet.
        if (state_==State::Pending ||
            (state_==State::Active &&
             !(request_.frontend==Frontend::Lua &&
               request_.completion==Request::Completion::NativeBoundary)))
            settle(result_.decoder_calls,true,0,0,0,0,0,0,Terminal::OwnerLost);
        invocation_=0;return true;
    }
    bool canEnqueue() const {return state_==State::Empty && !departing_;}
    State state() const {return state_;}
    uint64_t operation() const {return state_==State::Empty?0:result_.operation;}
    bool ownsActive(uint64_t operation) const {
        return operation && state_==State::Active && result_.operation==operation;
    }
    Context context(uint64_t invocation) {
        if (!invocation || state_==State::Pending || state_==State::Active) return {};
        if (invocation_!=invocation) {
            if (checkpoint_==UINT64_MAX) return {};
            invocation_=invocation;++checkpoint_;
        }
        return {invocation_,checkpoint_};
    }
    // Provisional identity, not ordinary permission to observe an Active slot.
    // Only the native stack-local event owner may service its reads.
    Context eventContext(Attachment attachment, uint64_t operation, uint64_t invocation) {
        if (!admits(attachment) || !ownsActive(operation) || !invocation ||
            checkpoint_==UINT64_MAX) return {};
        return {invocation,++checkpoint_};
    }
    // Native checkpoint installation, after successful final stop only.
    bool checkpointChanged(Context expected) {
        if (state_==State::Pending || state_==State::Active ||
            expected.invocation!=invocation_ || expected.checkpoint!=checkpoint_ ||
            !expected.invocation || checkpoint_==UINT64_MAX) return false;
        // A restored checkpoint replaces the state described by any retained
        // terminal result. Retire it rather than exposing pre-restore evidence
        // or leaving the only execution slot occupied by that old result.
        state_=State::Empty;request_={};result_={};stop_={};
        ++checkpoint_;return true;
    }
    Admission enqueue(Request request, Context current, bool safe) {
        if (state_==State::Pending || state_==State::Active) return Admission::Busy;
        if (state_==State::Terminal) return Admission::Capacity;
        if (!safe || !admits(request.attachment) || request.frontend!=frontend_)
            return Admission::UnsafeContext;
        if (!current.invocation || !current.checkpoint ||
            current.invocation!=invocation_ || current.checkpoint!=checkpoint_ ||
            request.expected.invocation!=current.invocation ||
            request.expected.checkpoint!=current.checkpoint)
            return Admission::StaleContext;
        if (!request.cycle_budget || request.cycle_budget>10000 ||
            !request.decoder_limit || !request.virtual_ms_limit || !request.wall_ms_limit ||
            serial_==UINT64_MAX || checkpoint_==UINT64_MAX)
            return Admission::Capacity;
        if (validateConditions(request.stop_any))return Admission::UnsafeContext;
        if (!request.stop_any.empty() && (!request.resume || request.over ||
            (request.completion!=Request::Completion::NativeBoundary && !request.normal_resume)))return Admission::UnsafeContext;
        stop_={};
        request_=request;attachment_lost_=false;
        result_={};result_.operation=++serial_;result_.frontend=request.frontend;
        result_.start=current;result_.attachment=request.attachment.serial;
        state_=State::Pending;return Admission::Accepted;
    }
    bool activate(uint64_t invocation, Request &request) {
        if (departing_ || state_!=State::Pending || invocation!=request_.expected.invocation) return false;
        state_=State::Active;request=request_;return true;
    }
    void setContinuing(bool continuing) {if (state_==State::Active) request_.resume=continuing;}
    const Request *pendingRequest() const {return state_==State::Pending?&request_:nullptr;}
    const Request *activeRequest() const {return state_==State::Active?&request_:nullptr;}
    StopRuntime &stopRuntime() {return stop_;}
    const std::vector<StopCondition> &conditions() const {return request_.stop_any;}
    uint32_t decoderCalls() const {return result_.decoder_calls;}
    void startBudget(uint64_t wall_ms, double virtual_ms) {wall_start_=wall_ms;virtual_start_=virtual_ms;}
    bool budgetExpired(uint64_t wall_ms, double virtual_ms) {
        if (state_!=State::Active || request_.completion==Request::Completion::NativeBoundary ||
            (request_.resume && !request_.log)) return false;
        if (result_.budget_exhausted) return true;
        if (result_.decoder_calls<request_.decoder_limit &&
            wall_ms-wall_start_<request_.wall_ms_limit &&
            virtual_ms-virtual_start_<request_.virtual_ms_limit) return false;
        result_.budget_exhausted=true;return true;
    }
    void decoderEntered() {if (state_==State::Active) ++result_.decoder_calls;}
    void settle(uint32_t calls, bool failed, uint16_t cs=0, uint32_t ip=0,
                uint16_t ax=0,uint16_t bx=0,uint16_t cx=0,uint16_t dx=0,
                Terminal terminal=Terminal::Held, uint64_t end_invocation=0) {
        if (state_!=State::Pending && state_!=State::Active) return;
        result_.stop=std::move(stop_.evidence);
        result_.cs=cs;result_.ip=ip;result_.ax=ax;result_.bx=bx;result_.cx=cx;result_.dx=dx;
        result_.decoder_calls=calls;result_.failed=failed || attachment_lost_;
        result_.terminal=attachment_lost_?Terminal::AttachmentLost:
            (terminal!=Terminal::Held?terminal:(failed?Terminal::Error:
             (result_.budget_exhausted?Terminal::Budget:Terminal::Held)));
        // The native frontier supplies the actual end owner for retained
        // descendant/parent continuations; legacy callers end at their start.
        // Start remains immutable. No borrowed native frame is retained here.
        if (end_invocation) invocation_=end_invocation;
        result_.end={invocation_,++checkpoint_};state_=State::Terminal;
    }
    const Result *result(Attachment attachment) const {
        return attachment.serial && state_==State::Terminal &&
            result_.attachment==attachment.serial && attachment_.serial==attachment.serial?&result_:nullptr;
    }
    bool release(Attachment attachment, uint64_t operation) {
        const auto *r=result(attachment);
        if (!r || r->operation!=operation) return false;
        state_=State::Empty;request_={};result_={};stop_={};
        return true; // Session remains attached.
    }
};
}
#endif
