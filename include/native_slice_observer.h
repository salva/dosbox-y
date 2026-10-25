/* GPL-2.0-or-later. Optional emulator-thread CPU-slice observation point. */
#ifndef DOSBOX_NATIVE_SLICE_OBSERVER_H
#define DOSBOX_NATIVE_SLICE_OBSERVER_H
namespace NativeExecution {
// Called immediately before a normal-loop or debugger decoder entry and after
// return/unwind, not inside instructions or callbacks. No host waiting/pumping occurs.
// This is a native extension point, NOT input/control admission or a wire API.
// A consumer owns its finite lifetime and must obey current resource/lease rules.
// In particular an observer must not throw, execute guest code, or pump services.
// noexcept is a caller contract; C++14 does not encode it in pointer types.
using SliceCallback = void (*)(void *, bool before);
class SliceObserver {
    SliceCallback callback_=nullptr;
    void *context_=nullptr;
    bool notifying_=false;
public:
    bool bind(SliceCallback callback,void *context) noexcept {
        if(!callback || callback_ || notifying_)return false;
        callback_=callback;context_=context;return true;
    }
    bool unbind(SliceCallback callback,void *context) noexcept {
        if(callback_!=callback || context_!=context || !callback)return false;
        callback_=nullptr;context_=nullptr;return true;
    }
    void notify(bool before) noexcept {
        if(!callback_ || notifying_)return;
        auto callback=callback_;auto context=context_;
        notifying_=true;callback(context,before);notifying_=false;
    }
};
inline SliceObserver &slice_observer() noexcept {static SliceObserver observer;return observer;}
}
#endif
