// Opt-in local diagnostic only. No guest state or input policy.
#ifndef DOSBOX_MOUSE_ROUTE_RECEIPT_H
#define DOSBOX_MOUSE_ROUTE_RECEIPT_H
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstdint>
namespace MouseRoute {
struct Sink {
    FILE *file = nullptr;
    unsigned count = 0;
    uint64_t request = 0;
    Sink() {
        const char *path = std::getenv("DOSBOX_Y_MOUSE_RECEIPT");
        // Exclusive create: never overwrite an earlier session's evidence.
        if (path && *path) file = std::fopen(path, "wx");
    }
    ~Sink() { if (file) std::fclose(file); }
};
inline Sink &sink() { static Sink s; return s; }
inline void record(const char *stage, const char *reason, int button=-1,
                   int edge=-1, int x=0, int y=0, int locked=-1, int relative=-1,
                   int result=-1) {
    auto &s = sink();
    if (!s.file || s.count >= 512) return;
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    // All strings are fixed implementation literals, never guest text.
    if (std::fprintf(s.file,
        "MR1 seq=%u request=%llu host_steady_us=%lld stage=%s reason=%s button=%d edge=%d x=%d y=%d locked=%d relative=%d result=%d\n",
        ++s.count, (unsigned long long)s.request, (long long)us, stage, reason,
        button, edge, x, y, locked, relative, result) < 0 || std::fflush(s.file)) {
        std::fclose(s.file); s.file = nullptr;
    }
}
inline void begin(int button, int edge, int x, int y, int locked, int relative) {
    auto &s = sink();
    if (s.file && s.count < 512) ++s.request;
    record("sdl", "requested", button, edge, x, y, locked, relative);
}
}
#endif
