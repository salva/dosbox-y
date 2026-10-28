/* GPL-2.0-or-later. Copied facts from one original-engine archive save. */
#ifndef DOSBOX_STATE_ARCHIVE_EFFECTS_H
#define DOSBOX_STATE_ARCHIVE_EFFECTS_H
#include <chrono>
namespace StateArchive {
// Cooperative host deadline, checked between complete owner operations.
// Never interrupt a serializer or replay uncertain effects.
class Budget {
    std::chrono::steady_clock::time_point end_;
public:
    explicit Budget(unsigned milliseconds) : end_(std::chrono::steady_clock::now() +
                                                   std::chrono::milliseconds(milliseconds)) {}
    bool expired() const { return std::chrono::steady_clock::now() >= end_; }
};
struct Effects {
    // Publication covers the main archive only. Flagged-file sidecars are
    // separate, and no directory-sync/transaction durability is promised.
    bool published = false;
    bool cleanup_attempted = false;
    bool cleanup_failed = false;
    bool timed_out = false;
};
}
#endif
