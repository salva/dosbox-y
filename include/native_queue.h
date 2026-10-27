/* GPL-2.0-or-later. Finite typed mutation ledger, no wire or host authority. */
#ifndef DOSBOX_NATIVE_QUEUE_H
#define DOSBOX_NATIVE_QUEUE_H
#include "native_ownership.h"
namespace NativeInput {
enum class State { Empty, Queued, Running, Dispatched, WaitingFrame, Encoding, Completed, Canceled, Expired, Failed };
enum class Error { None, BadRequest, InvalidTransition, Unsupported, NotRunning,
    Busy, Exhausted, DuplicateId, UnknownId, RetiredId, NotTerminal, TooLate,
    Expired, NoFrame, Canceled, DependencyInvalidated, ProfileChanged, IOError, QuotaExceeded, TransportClosed, CounterExhausted, ClockUncertain };
struct Event {
    enum class Kind { Edge, Viewport } kind=Kind::Edge;
    unsigned index=0;
    bool down=false;
    uint32_t x=0,y=0,after_ms=0;
};
struct Receipt {
    char target[65]={};
    uint64_t sequence=0,epoch=0,profile=0;
    State state=State::Empty;
    Error reason=Error::None;
    unsigned prefix=0;
    bool dispatched_time=false,terminal_time=false,has_cleanup=false;
    uint64_t dispatch_host=0,dispatch_guest=0,terminal_host=0,cleanup_generation=0;
};
struct Slot {
    bool capture=false;
    Receipt receipt;
    Event events[16];
    unsigned count=0;
    uint64_t deadline=0,start_guest=0;
    bool started=false;
};
struct PumpDispatch {
    Dispatch edges;
    bool (*motion)(uint32_t,uint32_t,void *) = nullptr;
    void *context=nullptr;
    bool (*profile)(uint64_t,void *)=nullptr;
    void (*submitted_motion)(const char *,void *)=nullptr;
};
class Queue {
    Slot slots_[64];
    char ids_[4096][65]={};
    unsigned lifetime_=0;
    unsigned pending_[64]={},pending_count_=0;
    uint64_t sequence_=0,epoch_=0,profile_=0;
    bool closed_=false;
    Ownership ownership_;
    Error cleanup_reason_=Error::None;
    static bool terminal(State state) {
        return state==State::Dispatched || state==State::Completed || state==State::Canceled ||
            state==State::Expired || state==State::Failed;
    }
    static bool id_valid(const char *id) {
        if(!id) return false;
        size_t n=0;
        for(;n<65 && id[n];n++) {
            char c=id[n];if(!((c>='A' && c<='Z')||(c>='a' && c<='z')||
                (c>='0' && c<='9')||c=='_'||c=='-')) return false;
        }
        return n>=1 && n<=64;
    }
    int slot(const char *id) const {
        if(!id_valid(id)) return -1;
        for(unsigned i=0;i<64;i++) if(slots_[i].receipt.state!=State::Empty &&
            !strcmp(slots_[i].receipt.target,id)) return int(i);
        return -1;
    }
    bool retired(const char *id) const {
        if(!id_valid(id)) return false;
        for(unsigned i=0;i<lifetime_;i++) if(!strcmp(ids_[i],id)) return true;
        return false;
    }
    void pop() {
        for(unsigned i=1;i<pending_count_;i++) pending_[i-1]=pending_[i];
        --pending_count_;
    }
public:
    const Ownership &ownership() const {return ownership_;}
    Error cleanup_reason() const {return cleanup_reason_;}
    uint64_t epoch() const {return epoch_;}
    bool closed() const {return closed_;}
    bool checkpoint_quiescent() const {
        if (closed_ || pending_count_ || ownership_.any() || ownership_.blocked()) return false;
        for (const auto &s : slots_) if (s.receipt.state != State::Empty) return false;
        return true;
    }
    bool checkpoint_epoch() {
        if (!checkpoint_quiescent() || epoch_==UINT64_MAX) return false;
        ++epoch_; return true;
    }
    unsigned pending() const {return pending_count_;}
    unsigned lifetime() const {return lifetime_;}
    uint64_t capture_deadline() const {return pending_count_?slots_[pending_[0]].deadline:0;}
    void fault(Error reason,uint64_t host,uint64_t guest) {
        abort(reason,pending_count_?int(pending_[0]):-1,host,guest);
    }
    Error poll(const char *id,const Receipt *&receipt) const {
        receipt=nullptr;int i=slot(id);
        if(i<0) return retired(id)?Error::RetiredId:Error::UnknownId;
        receipt=&slots_[i].receipt;return Error::None;
    }
    Error ack(const char *id) {
        int i=slot(id);if(i<0) return retired(id)?Error::None:Error::UnknownId;
        if(!terminal(slots_[i].receipt.state)) return Error::NotTerminal;
        slots_[i]=Slot();return Error::None;
    }
    // Conservative invalidation: pending prefixes persist, every suffix ends.
    void abort(Error reason,int target,uint64_t host,uint64_t guest) {
        if(!pending_count_ && !ownership_.any() && !ownership_.blocked()) return;
        if(epoch_==UINT64_MAX) closed_=true;else ++epoch_;
        cleanup_reason_=reason;
        ownership_.request(host,guest,true);
        for(unsigned j=0;j<pending_count_;j++) {
            unsigned i=pending_[j];auto &r=slots_[i].receipt;
            r.reason=int(i)==target ? reason : Error::DependencyInvalidated;
            r.state=r.reason==Error::Canceled?State::Canceled:
                (r.reason==Error::Expired || r.reason==Error::NoFrame)?State::Expired:State::Failed;
            r.terminal_host=host;r.terminal_time=true;r.has_cleanup=true;
            r.cleanup_generation=ownership_.receipt().generation;
        }
        pending_count_=0;
    }
    Error cancel(const char *id,uint64_t host,uint64_t guest) {
        int i=slot(id);if(i<0) return retired(id)?Error::RetiredId:Error::UnknownId;
        auto &r=slots_[i].receipt;
        if(r.state==State::Canceled) return Error::None;
        if(terminal(r.state) || r.state==State::Encoding) return Error::TooLate;
        abort(Error::Canceled,i,host,guest);return Error::None;
    }
    void release(uint64_t host,uint64_t guest) {abort(Error::DependencyInvalidated,-1,host,guest);}
    // Caller guarantees valid selected SDL profile and generation before admission
    // and pump. A profile failure rejects keys/buttons too, not just motion.
    Error admit(const char *id,const Event *events,unsigned count,uint32_t deadline_ms,
                uint64_t host,uint64_t guest,uint64_t profile,bool supported,bool running) {
        (void)guest;
        if(!id_valid(id)||!events||count<1||count>16||deadline_ms<1||deadline_ms>2000||
            host>UINT64_MAX-deadline_ms) return Error::BadRequest;
        if(retired(id)) return Error::DuplicateId;
        if(closed_||lifetime_==4096) return Error::Exhausted;
        if(!running) return Error::NotRunning;
        if(!supported) return Error::Unsupported;
        if(ownership_.blocked()) return Error::Busy;
        if((pending_count_ || ownership_.any()) && profile!=profile_) return Error::Busy;
        int free=-1;for(unsigned i=0;i<64;i++) if(slots_[i].receipt.state==State::Empty) {free=int(i);break;}
        if(free<0) return Error::Busy;
        bool predicted[EdgeCount];for(unsigned i=0;i<EdgeCount;i++) predicted[i]=ownership_.held(i);
        // Replay only unexecuted suffixes from ACTUAL H, never full original batches.
        for(unsigned q=0;q<pending_count_;q++) {
            const auto &s=slots_[pending_[q]];
            for(unsigned e=s.receipt.prefix;e<s.count;e++) if(s.events[e].kind==Event::Kind::Edge)
                predicted[s.events[e].index]=s.events[e].down;
        }
        uint32_t previous=0;
        for(unsigned i=0;i<count;i++) {
            const auto &e=events[i];
            if(e.after_ms>1000||(i && e.after_ms<previous)) return Error::BadRequest;
            previous=e.after_ms;
            if(e.kind==Event::Kind::Edge) {
                if(e.index>=EdgeCount) return Error::Unsupported;
                if(predicted[e.index]==e.down) return Error::InvalidTransition;
                predicted[e.index]=e.down;
            } else if(e.kind==Event::Kind::Viewport) {
                if(e.x>65535||e.y>65535) return Error::BadRequest;
            } else return Error::BadRequest;
        }
        if(sequence_==UINT64_MAX) {closed_=true;return Error::CounterExhausted;}
        profile_=profile;auto &s=slots_[unsigned(free)];s=Slot();
        strcpy(s.receipt.target,id);s.receipt.sequence=++sequence_;s.receipt.epoch=epoch_;
        s.receipt.profile=profile;s.receipt.state=State::Queued;
        memcpy(s.events,events,count*sizeof(Event));s.count=count;s.deadline=host+deadline_ms;
        strcpy(ids_[lifetime_++],id);pending_[pending_count_++]=unsigned(free);return Error::None;
    }
    // Capture shares the SAME admitted IDs, sequence, slots and FIFO; no second ledger.
    Error admit_capture(const char *id,uint32_t timeout,uint64_t host,uint64_t guest,
                        uint64_t profile,bool running) {
        // Reserve through common admission with a no-effect typed viewport marker.
        // No input profile requirement: renderer support is checked at selection.
        Event marker;marker.kind=Event::Kind::Viewport;
        Error error=admit(id,&marker,1,timeout,host,guest,profile,true,running);
        if(error!=Error::None) return error;
        int i=slot(id);slots_[unsigned(i)].capture=true;slots_[unsigned(i)].count=0;
        return Error::None;
    }
    // Native frame owner may select only current FIFO waiting capture. Caller
    // supplies newer eligible frame/renderer constraints; no PNG completion inferred.
    bool select_capture(uint64_t host,uint64_t guest,const Receipt *&receipt) {
        receipt=nullptr;
        if(!pending_count_) return false;
        auto &s=slots_[pending_[0]];
        if(!s.capture || s.receipt.state!=State::WaitingFrame) return false;
        if(host>=s.deadline) {abort(Error::NoFrame,int(pending_[0]),host,guest);return false;}
        s.receipt.state=State::Encoding;receipt=&s.receipt;return true;
    }
    bool complete_capture(bool ok,uint64_t host,uint64_t guest) {
        if(!pending_count_) return false;
        auto &s=slots_[pending_[0]];
        if(!s.capture || s.receipt.state!=State::Encoding) return false;
        if(!ok) {abort(Error::IOError,int(pending_[0]),host,guest);return true;}
        s.receipt.state=State::Completed;s.receipt.terminal_host=host;s.receipt.terminal_time=true;pop();return true;
    }
    // Transport/control owner invokes fault/release/cancel before this due-event
    // phase. Profile/stop, lease/deadline and cleanup have precedence over events.
    void pump(uint64_t host,uint64_t guest,uint64_t profile,bool supported,
              bool running,PumpDispatch dispatch,bool execute=true, bool stopped_cleanup=false) {
        bool input_pending=ownership_.any();
        for(unsigned i=0;i<pending_count_;i++) if(!slots_[pending_[i]].capture) input_pending=true;
        if((pending_count_ || (ownership_.any() && !ownership_.blocked())) && (!running || (!supported && input_pending) || profile!=profile_))
            abort(!running?Error::NotRunning:Error::ProfileChanged,pending_count_?int(pending_[0]):-1,host,guest);
        if(!ownership_.blocked() && ownership_.expired(host,guest)) abort(Error::Expired,pending_count_?int(pending_[0]):-1,host,guest);
        for(unsigned q=0;q<pending_count_;q++) if(host>=slots_[pending_[q]].deadline) {
            abort(slots_[pending_[q]].capture?Error::NoFrame:Error::Expired,int(pending_[q]),host,guest);break;
        }
        // Cleanup-only permission never enables due events or guest execution.
        ownership_.clean(running || stopped_cleanup,host,guest,dispatch.edges);
        if(!running || (!supported && input_pending) || ownership_.blocked() || closed_) return;
        if(!execute) return;
        // Fixed max64*16 calls; no waiting, sleeping or allocations.
        while(pending_count_) {
            auto &s=slots_[pending_[0]];auto &r=s.receipt;
            if(s.capture) {
                if(r.state==State::Queued) {
                    r.state=State::WaitingFrame;r.dispatch_host=host;r.dispatch_guest=guest;r.dispatched_time=true;
                }
                return; // capture blocks later mutations until checked completion
            }
            if(!s.started) {s.started=true;s.start_guest=guest;}
            if(guest<s.start_guest) {abort(Error::CounterExhausted,int(pending_[0]),host,guest);return;}
            while(r.prefix<s.count && guest-s.start_guest>=s.events[r.prefix].after_ms) {
                // Guard EACH event, including up, before native primitive.
                // False means profile mismatch, not primitive IO failure.
                if(dispatch.profile && !dispatch.profile(r.profile,dispatch.context)) {
                    abort(Error::ProfileChanged,int(pending_[0]),host,guest);
                    ownership_.clean(running,host,guest,dispatch.edges);return;
                }
                const auto &e=s.events[r.prefix];bool ok=false;
                if(e.kind==Event::Kind::Edge) ok=ownership_.edge(e.index,e.down,host,guest,dispatch.edges);
                else if(dispatch.motion) {
                    try {if(dispatch.submitted_motion)dispatch.submitted_motion(r.target,dispatch.context);ok=dispatch.motion(e.x,e.y,dispatch.context);if(dispatch.submitted_motion)dispatch.submitted_motion(nullptr,dispatch.context);} catch(...) {if(dispatch.submitted_motion)dispatch.submitted_motion(nullptr,dispatch.context);ok=false;}
                }
                if(!ok) {abort(Error::IOError,int(pending_[0]),host,guest);ownership_.clean(running,host,guest,dispatch.edges);return;}
                if(r.prefix==0) {r.state=State::Running;r.dispatch_host=host;r.dispatch_guest=guest;r.dispatched_time=true;}
                ++r.prefix;
            }
            if(r.prefix<s.count) return;
            r.state=State::Dispatched;r.terminal_host=host;r.terminal_time=true;pop();
        }
    }
};
}
#endif
