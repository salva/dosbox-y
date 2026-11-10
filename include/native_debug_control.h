/* GPL-2.0-or-later. One copied native control exchange; no guest authority on workers. */
#ifndef DOSBOX_NATIVE_DEBUG_CONTROL_H
#define DOSBOX_NATIVE_DEBUG_CONTROL_H
#include "native_debug_execution.h"
#include <atomic>
#include <mutex>
namespace NativeDebug {
struct Control {
    enum class Kind { Observe, Pause };
    Kind kind=Kind::Observe;
    Attachment attachment;
    uint64_t operation=0;
};
class ControlMailbox {
    std::atomic<bool> pending_{false};
    std::mutex mutex_;
    bool occupied_=false, ready_=false;
    Control request_;
    Status response_;
public:
    // Caller owns the single exchange until take(). Busy never replaces it.
    bool submit(Control request) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (occupied_) return false;
        occupied_=true;request_=request;
        pending_.store(true,std::memory_order_release);
        return true;
    }
    bool pending() const {return pending_.load(std::memory_order_acquire);}
    template<class Consumer> void service(Consumer consume) {
        if (!pending()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pending_.load(std::memory_order_relaxed)) return;
        response_=consume(request_);ready_=true;
        pending_.store(false,std::memory_order_release);
    }
    bool take(Status &response) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ready_) return false;
        response=response_;ready_=occupied_=false;return true;
    }
};
}
#endif
