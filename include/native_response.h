/* GPL-2.0-or-later. Checked fixed-size v1 JSON responses, no transport effects. */
#ifndef DOSBOX_NATIVE_RESPONSE_H
#define DOSBOX_NATIVE_RESPONSE_H
#include "live_request.h"
#include <stdio.h>
#include <stdarg.h>
namespace NativeInput {
struct FrameReceipt {
    uint64_t sequence=0,guest_ms=0,bytes=0,encode_start_ms=0,encode_end_ms=0;
    uint32_t source_w=0,source_h=0,bpp=0,flags=0,png_w=0,png_h=0;
    bool published=false,late=false,quarantined=false;
    char sha256[65]={},basename[64]={},quarantine_basename[64]={};
};
class Response {
    char bytes_[4096]={};size_t used_=0;bool ok_=true;
    void add(const char *format,...) {
        if(!ok_)return;
        va_list args;va_start(args,format);
        int n=vsnprintf(bytes_+used_,sizeof(bytes_)-used_,format,args);va_end(args);
        if(n<0 || size_t(n)>=sizeof(bytes_)-used_){ok_=false;return;}
        used_+=size_t(n);
    }
    // Never echo caller data without JSON escaping and a strict maximum.
    void string(const char *s,size_t maximum) {
        if(!s){add("null");return;}
        add("\"");size_t n=0;
        for(;n<maximum && s[n];n++) {
            unsigned char c=static_cast<unsigned char>(s[n]);
            if(c=='"'||c=='\\')add("\\%c",c);
            else if(c<32)add("\\u%04x",unsigned(c));
            else if(c>=128){ok_=false;return;} // all public emitted strings ASCII
            else add("%c",c);
        }
        if(s[n]){ok_=false;return;}add("\"");
    }
    void counter(uint64_t value,bool present=true) {
        if(!present)add("null");else add("\"%llu\"",static_cast<unsigned long long>(value));
    }
public:
    static const char *error(Error error) {
        switch(error) {
#define E(a,b) case Error::a:return b
            E(None,nullptr);E(BadRequest,"bad_request");E(InvalidTransition,"invalid_transition");
            E(Unsupported,"unsupported");E(NotRunning,"not_running");E(Busy,"busy");
            E(Exhausted,"exhausted");E(DuplicateId,"duplicate_id");E(UnknownId,"unknown_id");
            E(RetiredId,"retired_id");E(NotTerminal,"not_terminal");E(TooLate,"too_late");
            E(Expired,"expired");E(NoFrame,"no_frame");E(Canceled,"canceled");
            E(DependencyInvalidated,"dependency_invalidated");E(ProfileChanged,"profile_changed");
            E(IOError,"io_error");E(QuotaExceeded,"quota_exceeded");E(TransportClosed,"transport_closed");
            E(CounterExhausted,"counter_exhausted");E(ClockUncertain,"clock_uncertain");
#undef E
        }return nullptr;
    }
    static const char *operation(Operation op) {
        switch(op){case Operation::None:return nullptr;case Operation::Input:return "input";
            case Operation::Capture:return "capture";case Operation::Poll:return "poll";
            case Operation::Cancel:return "cancel";case Operation::Ack:return "ack";
            case Operation::Release:return "release";}return nullptr;
    }
    static const char *state(State state) {
        switch(state){case State::Empty:return nullptr;case State::Queued:return "queued";
            case State::Running:return "running";case State::Dispatched:return "dispatched";
            case State::WaitingFrame:return "waiting_frame";case State::Encoding:return "encoding";
            case State::Completed:return "completed";case State::Canceled:return "canceled";
            case State::Expired:return "expired";case State::Failed:return "failed";}return nullptr;
    }
    void begin(const char *instance,const char *id,Operation op,Error failure) {
        used_=0;ok_=true;
        add("{\"v\":1,\"id\":");string(id,64);
        add(",\"instance\":");string(instance,64);
        add(",\"op\":");string(operation(op),16);
        add(",\"ok\":%s,\"error\":",failure==Error::None?"true":"false");string(error(failure),32);
        add(",\"data\":");
    }
    void null_data(){add("null");}
    void admission(const Receipt &r) {
        add("{\"target\":");string(r.target,64);add(",\"seq\":");counter(r.sequence);
        add(",\"epoch\":");counter(r.epoch);add(",\"profile\":");counter(r.profile);
        add(",\"state\":\"queued\"}");
    }
    // Publication owner supplies frame after selection, including failed/late evidence.
    bool receipt(const Receipt &r,const FrameReceipt *frame=nullptr) {
        if((r.state==State::Encoding || r.state==State::Completed) && !frame) {
            ok_=false;return false;
        }
        add("{\"target\":");string(r.target,64);add(",\"seq\":");counter(r.sequence);
        add(",\"epoch\":");counter(r.epoch);add(",\"profile\":");counter(r.profile);
        add(",\"state\":");string(state(r.state),16);add(",\"reason\":");string(error(r.reason),32);
        add(",\"prefix\":%u,\"dispatch_host_ms\":",r.prefix);
        counter(r.dispatch_host,r.dispatched_time);add(",\"dispatch_guest_ms\":");counter(r.dispatch_guest,r.dispatched_time);
        add(",\"terminal_host_ms\":");counter(r.terminal_host,r.terminal_time);
        add(",\"cleanup_generation\":");counter(r.cleanup_generation,r.has_cleanup);
        add(",\"frame\":");
        if(!frame) add("null");
        else {
            add("{\"frame_seq\":");counter(frame->sequence);
            add(",\"encode_start_ms\":");counter(frame->encode_start_ms);
            add(",\"encode_end_ms\":");counter(frame->encode_end_ms);
            add(",\"guest_ms\":");counter(frame->guest_ms);
            add(",\"source_w\":%u,\"source_h\":%u,\"bpp\":%u,\"flags\":%u,\"png_w\":%u,\"png_h\":%u,\"bytes\":",
                frame->source_w,frame->source_h,frame->bpp,frame->flags,frame->png_w,frame->png_h);
            counter(frame->bytes,frame->published);
            add(",\"sha256\":");string(frame->published?frame->sha256:nullptr,64);
            add(",\"basename\":");string(frame->published?frame->basename:nullptr,63);
            add(",\"late\":%s,\"quarantine\":",frame->late?"true":"false");
            if(frame->quarantined) {
                add("{\"generated_basename\":");string(frame->quarantine_basename,63);
                add(",\"reason\":\"unlink_failed\"}");
            } else add("null");
            add("}");
        }
        add("}");return ok_;
    }
    void cleanup(const Cleanup &c,const char *reason) {
        static const char *states[]={"idle","pending","deferred","dispatched","failed"};
        add("{\"generation\":");counter(c.generation);
        add(",\"reason\":");string(reason,32);add(",\"state\":");
        unsigned state_index=static_cast<unsigned>(c.state);
        if(state_index>=5 || c.attempts>EdgeCount){ok_=false;return;}
        string(states[state_index],16);
        add(",\"attempted\":[");
        for(unsigned i=0;i<c.attempts;i++){if(i)add(",");held_name(c.attempted[i]);}
        add("],\"remaining\":[");bool comma=false;
        for(unsigned i=0;i<EdgeCount;i++)if(c.remaining[i]){if(comma)add(",");held_name(i);comma=true;}
        add("],\"host_ms\":");counter(c.host_ms);add(",\"guest_ms\":");counter(c.guest_ms);add("}");
    }
private:
    void held_name(unsigned index) {
        if(index<KeyCount){add("{\"key\":");string(KeyVocabulary[index].name,16);add("}");}
        else if(index<EdgeCount)add("{\"button\":%u}",index-KeyCount);
        else ok_=false;
    }
public:
    void retired(const char *target){add("{\"target\":");string(target,64);add(",\"outcome\":\"retired\"}");}
    bool end(){add("}\n");return ok_;}
    const char *data() const {return ok_?bytes_:nullptr;}
    size_t size() const {return ok_?used_:0;}
};
}
#endif
