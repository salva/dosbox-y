/* GPL-2.0-or-later. One emulator-thread pending capture, no wire admission. */
#ifndef DOSBOX_CAPTURE_PENDING_H
#define DOSBOX_CAPTURE_PENDING_H
#include "capture_publish.h"
#include "capture_frame.h"
namespace CapturePNG {
enum class CaptureState { Idle, Waiting, Encoding, Completed, Expired, Canceled, Failed };
class Pending {
    CaptureState state_ = CaptureState::Idle;
    int root_ = -1;
    uint64_t sequence_ = 0, after_frame_ = 0, deadline_ = 0;
    size_t limit_ = 0;
    Publication result_;
    uint64_t frame_ = 0;
    uint32_t tick_ = 0;
    bool late_ = false;
public:
    bool admit(int root, uint64_t sequence, uint64_t after_frame,
               uint64_t host_ms, uint32_t timeout, size_t limit) {
        if (state_ != CaptureState::Idle || !sequence || after_frame==UINT64_MAX ||
            timeout<1 || timeout>2000 || host_ms>UINT64_MAX-timeout ||
            !limit || limit>16u*1024u*1024u) return false;
        struct stat st;
        if (fstat(root,&st)!=0 || !S_ISDIR(st.st_mode)) return false;
        root_=root;sequence_=sequence;after_frame_=after_frame;
        deadline_=host_ms+timeout;limit_=limit;state_=CaptureState::Waiting;
        return true;
    }
    void pump(uint64_t host_ms) {
        if (state_==CaptureState::Waiting && host_ms>=deadline_) state_=CaptureState::Expired;
    }
    bool cancel() {
        if (state_==CaptureState::Canceled) return true;
        if (state_!=CaptureState::Waiting) return false;
        state_=CaptureState::Canceled; return true;
    }
    // Completion host time is obtained after checked encode/close/hash, not trigger.
    // Production owner supplies monotonic clock; fixture may supply a clock double.
    void frame(const CaptureFrame::View &view, uint64_t host_ms,
               uint64_t (*now)(void *), void *context) {
        pump(host_ms);
        if (state_!=CaptureState::Waiting || view.sequence<=after_frame_ || !now) return;
        state_=CaptureState::Encoding;frame_=view.sequence;tick_=view.guest_tick;
        result_=publish(root_,sequence_,frame_,view.width,view.height,view.bpp,
            view.pitch,view.flags&1u,view.flags&2u,view.pixels,view.palette,limit_);
        late_=now(context)>=deadline_;
        state_=result_.completed ? CaptureState::Completed : CaptureState::Failed;
    }
    CaptureState state() const { return state_; }
    const Publication &result() const { return result_; }
    uint64_t selected_frame() const { return frame_; }
    uint32_t selected_tick() const { return tick_; }
    bool late() const { return late_; }
};
}
#endif
