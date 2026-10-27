/* GPL-2.0-or-later. Shared emulator task owner, independent of transport,
 * syntax and Lua. Commands are admitted by their owning dispatcher; delayed
 * primitives/render publication never call Lua. No framebuffer retention.
 */
#ifndef DOSBOX_LIVE_TASKS_H
#define DOSBOX_LIVE_TASKS_H
#include "native_response.h"
#include "live_control.h"
#include "native_clock.h"
#include "capture_publish.h"
#include "capture_frame.h"
namespace NativeInput {
struct EndpointContext {
    uint64_t host=0,profile=0;
    uint32_t tick=0;
    bool running=false,supported=false,frame_exhausted=false;
    PumpDispatch dispatch;
};
struct TaskResult {
    Error error=Error::None;
    const Receipt *receipt=nullptr;
    const FrameReceipt *frame=nullptr;
    bool admission=false,retired=false,cleanup=false;
};
class Tasks {
    Queue queue_;
    GuestClock clock_;
    FrameReceipt frames_[64];
    uint64_t frame_ids_[64]={};
    int root_;
    size_t limit_;
    EndpointContext current_;
    const char *cleanup_reason_=nullptr;
    bool closed_fault_=false;
    bool frame_budget_=false;
    uint64_t last_frame_=0;
    FrameReceipt *frame(uint64_t sequence) {
        for(unsigned i=0;i<64;i++)if(frame_ids_[i]==sequence)return &frames_[i];
        return nullptr;
    }
    void fault(Error reason) {
        cleanup_reason_=Response::error(reason);
        queue_.fault(reason,current_.host,clock_.guest_ms());
    }
public:
    Tasks(int root,size_t limit):root_(-1),limit_(limit) {
        // Own the directory identity, not an inherited descriptor number that
        // full-library Lua or a C module can close and reuse after startup.
        if(limit && limit<=16u*1024u*1024u)
            root_=fcntl(root,F_DUPFD_CLOEXEC,0);
        if(root_<0)closed_fault_=true;
    }
    ~Tasks() {if(root_>=0)close(root_);}
    Tasks(const Tasks &)=delete;
    Tasks &operator=(const Tasks &)=delete;
    uint64_t epoch() const {return queue_.epoch();}
    LiveTaskStatus status() const {
        LiveTaskStatus s;s.configured=true;s.closed=closed();s.pending=queue_.pending();
        s.lifetime=queue_.lifetime();s.epoch=queue_.epoch();s.guest_ms=clock_.guest_ms();
        s.clock_error=unsigned(clock_.error());s.frame_exhausted=current_.frame_exhausted;
        for(unsigned i=0;i<EdgeCount;i++)if(queue_.ownership().held(i))++s.held;
        for(unsigned i=0;i<64;i++)if(frame_ids_[i])++s.frames;
        s.cleanup_generation=queue_.ownership().receipt().generation;
        s.cleanup_state=unsigned(queue_.ownership().receipt().state);return s;
    }
    bool checkpoint_ready() const { return !closed() && queue_.checkpoint_quiescent(); }
    bool checkpoint_rebase(uint64_t host, uint32_t tick) {
        return checkpoint_ready() && queue_.checkpoint_epoch() && clock_.checkpoint_rebase(host,tick);
    }
    bool closed() const {return closed_fault_||queue_.closed();}
    // Local native owner terminal collection after closed transport; no wire API.
    const Receipt *retained_receipt(const char *id) const {
        const Receipt *receipt=nullptr;queue_.poll(id,receipt);return receipt;
    }
    const Cleanup &retained_cleanup() const {return queue_.ownership().receipt();}
    void invalidate(Error reason) {fault(reason);}
    void channel_fault(Error reason=Error::TransportClosed) {
        if(!closed_fault_) {fault(reason);closed_fault_=true;}
        // Capture publication is synchronous on this same native thread. Once
        // the channel is closed there is no remaining borrower of the root;
        // retain terminal receipts/input cleanup, not unused file authority.
        if(root_>=0) {close(root_);root_=-1;}
    }
    TaskResult execute(const Request &request,Error error=Error::None) {
        TaskResult result;
        const Receipt *receipt=nullptr;
        bool admission=false,retired=false,cleanup=false;
        uint64_t guest=clock_.guest_ms();
        if(error==Error::None) {
            switch(request.op) {
            case Operation::Input:
                error=queue_.admit(request.id,request.events,request.count,request.deadline_ms,
                    current_.host,guest,current_.profile,current_.supported,current_.running);
                admission=error==Error::None;break;
            case Operation::Capture:
                error=queue_.admit_capture(request.id,request.deadline_ms,current_.host,guest,
                    current_.profile,current_.running);admission=error==Error::None;break;
            case Operation::Poll:
                if(!strcmp(request.target,"@cleanup"))cleanup=true;
                else error=queue_.poll(request.target,receipt);
                break;
            case Operation::Cancel:
                error=queue_.cancel(request.target,current_.host,guest);
                if(error==Error::None){cleanup_reason_="canceled";error=queue_.poll(request.target,receipt);}break;
            case Operation::Ack: {
                const Receipt *prior=nullptr;
                queue_.poll(request.target,prior);uint64_t seq=prior?prior->sequence:0;
                error=queue_.ack(request.target);retired=error==Error::None;
                if(retired && seq)for(unsigned i=0;i<64;i++)if(frame_ids_[i]==seq){frame_ids_[i]=0;frames_[i]=FrameReceipt();}
                break;
            }
            case Operation::Release:
                queue_.release(current_.host,guest);cleanup_reason_="dependency_invalidated";cleanup=true;break;
            case Operation::None:error=Error::BadRequest;break;
            }
            if(admission)queue_.poll(request.id,receipt);
        }
        // Controls/abort cleanup execute before fresh data/events, no free pause.
        queue_.pump(current_.host,guest,current_.profile,current_.supported,
            current_.running,current_.dispatch,false);
        result.error=error;result.receipt=receipt;result.admission=admission;
        result.retired=retired;result.cleanup=cleanup;
        if(receipt)result.frame=frame(receipt->sequence);
        return result;
    }
    void format(Response &response,const Request &request,const TaskResult &result) {
        if(result.error!=Error::None)response.null_data();
        else if(result.admission && result.receipt)response.admission(*result.receipt);
        else if(result.cleanup)response.cleanup(queue_.ownership().receipt(),Response::error(queue_.cleanup_reason()));
        else if(result.retired)response.retired(request.target);
        else if(result.receipt)response.receipt(*result.receipt,result.frame);
        else response.null_data();
    }
    void pump(EndpointContext context,bool dispatch=true, bool stopped_cleanup=false) {
        current_=context;
        if(!closed_fault_ && context.frame_exhausted) {
            fault(Error::CounterExhausted);closed_fault_=true;
        }
        if(!closed_fault_ && !clock_.observe(context.host,context.tick)) {
            fault(clock_.error()==ClockError::Exhausted?Error::CounterExhausted:Error::ClockUncertain);
            closed_fault_=true;
        }
        // Required deadlines/profile/stop phase BEFORE decoded controls.
        queue_.pump(context.host,clock_.guest_ms(),context.profile,context.supported,
            context.running,context.dispatch,false,stopped_cleanup);
        // Closed endpoint still attempts/defer owned releases at safe pump.
        queue_.pump(context.host,clock_.guest_ms(),context.profile,context.supported,
            context.running,context.dispatch,dispatch && !closed(),stopped_cleanup);
        frame_budget_=!closed() && context.running;
    }
    // Native render owner passes only eligible new views. First frame after
    // waiting dispatch can satisfy capture; real file encoding is synchronous.
    void completed_frame(const CaptureFrame::View &view,EndpointContext context,
                         uint64_t (*now)(void *),void *clock_context,
                         CapturePNG::IO io=CapturePNG::IO(),CapturePNG::FileIO fileio=CapturePNG::FileIO()) {
        if(!frame_budget_||closed()||!context.running||!now||!view.sequence||view.sequence<=last_frame_)return;
        last_frame_=view.sequence;
        current_=context;
        if(!clock_.observe(context.host,context.tick)) {
            fault(clock_.error()==ClockError::Exhausted?Error::CounterExhausted:Error::ClockUncertain);
            closed_fault_=true;return;
        }
        const Receipt *receipt=nullptr;
        if(!queue_.select_capture(context.host,clock_.guest_ms(),receipt))return;
        FrameReceipt *f=nullptr;
        for(unsigned i=0;i<64;i++)if(!frame_ids_[i]){frame_ids_[i]=receipt->sequence;f=&frames_[i];break;}
        if(!f){queue_.complete_capture(false,context.host,clock_.guest_ms());return;}
        f->sequence=view.sequence;f->guest_ms=clock_.guest_ms();f->source_w=view.width;
        f->source_h=view.height;f->bpp=view.bpp;f->flags=view.flags;
        f->encode_start_ms=now(clock_context);
        auto publication=CapturePNG::publish(root_,receipt->sequence,view.sequence,
            view.width,view.height,view.bpp,view.pitch,view.flags&1u,view.flags&2u,
            view.pixels,view.palette,limit_,io,fileio);
        f->quarantined=publication.quarantined;
        memcpy(f->quarantine_basename,publication.quarantine_basename,sizeof(f->quarantine_basename));
        f->published=publication.completed;f->bytes=publication.bytes;
        f->png_w=publication.width;f->png_h=publication.height;
        memcpy(f->sha256,publication.sha256,sizeof(f->sha256));
        memcpy(f->basename,publication.basename,sizeof(f->basename));
        uint64_t host_after=now(clock_context);f->encode_end_ms=host_after;
        f->late=host_after>=queue_.capture_deadline();
        queue_.complete_capture(publication.completed,host_after,clock_.guest_ms());
    }
};
}
#endif
