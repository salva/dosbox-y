/* GPL-2.0-or-later. Bounded endpoint clock extension, never inferred wraps. */
#ifndef DOSBOX_NATIVE_CLOCK_H
#define DOSBOX_NATIVE_CLOCK_H
#include <stdint.h>
namespace NativeInput {
enum class ClockError { None, Uncertain, Exhausted };
class GuestClock {
    uint64_t host_=0,extended_=0;
    uint32_t tick_=0;
    bool initialized_=false;
    ClockError error_=ClockError::None;
public:
    explicit GuestClock(uint64_t initial=0) : extended_(initial) {}
    bool observe(uint64_t host,uint32_t tick) {
        if(error_!=ClockError::None) return false;
        if(!initialized_) {initialized_=true;host_=host;tick_=tick;return true;}
        if(host<host_ || host-host_>=uint64_t(1)<<31) {error_=ClockError::Uncertain;return false;}
        uint32_t delta=tick-tick_; // specified unsigned modular single-wrap delta
        if(extended_>UINT64_MAX-delta) {error_=ClockError::Exhausted;return false;}
        extended_+=delta;tick_=tick;host_=host;return true;
    }
    // Explicit instrumented rewind, only admitted while tasks are quiescent.
    // Preserve spent/extended time; change only the next delta baseline.
    bool checkpoint_rebase(uint64_t host, uint32_t tick) {
        if (error_ != ClockError::None || (initialized_ && host < host_)) return false;
        host_=host; tick_=tick; initialized_=true; return true;
    }
    uint64_t guest_ms() const {return extended_;}
    ClockError error() const {return error_;}
};
}
#endif
