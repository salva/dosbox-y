/* GPL-2.0-or-later. Native completed-render identity, not PNG completion. */
#ifndef DOSBOX_CAPTURE_FRAME_H
#define DOSBOX_CAPTURE_FRAME_H
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
namespace CaptureFrame {
struct View {
    uint64_t sequence;
    uint32_t guest_tick;
    uint32_t width, height, bpp, flags;
    size_t pitch;
    const uint8_t *pixels;
    const uint8_t *palette;
};
typedef void (*Observer)(const View &, void *);
// Emulator-thread-only; no retained framebuffer pointers or dynamic allocation.
// An observer is a native bounded capture owner, never a Lua/host callback.
class Clock {
    uint64_t sequence_ = 0;
    bool exhausted_ = false;
public:
    explicit Clock(uint64_t initial = 0) : sequence_(initial) {}
    bool end(bool updating, bool active, bool disabled, bool abort,
             View view, Observer observer, void *context) {
        if (!updating || !active || disabled || abort || exhausted_) return false;
        if (sequence_ == UINT64_MAX) { exhausted_ = true; return false; }
        view.sequence = ++sequence_;
        if (observer) observer(view, context);
        return true;
    }
    uint64_t sequence() const { return sequence_; }
    bool exhausted() const { return exhausted_; }
};
}
#endif
