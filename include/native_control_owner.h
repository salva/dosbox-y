/* GPL-2.0-or-later. Emulator-thread native exclusive control admission. */
#ifndef DOSBOX_NATIVE_CONTROL_OWNER_H
#define DOSBOX_NATIVE_CONTROL_OWNER_H
#include "machine_service.h"
namespace NativeControl {
struct Expected {
    uint64_t token = 0, epoch = 0;
};
struct Lease {
    uint64_t token = 0, epoch = 0, generation = 0;
};
enum class Admission { ok, busy, wrong_owner, stale_epoch, not_ready, limit };
class Owner {
    Lease lease_;
    uint64_t generation_ = 0;
public:
    bool busy() const { return lease_.generation != 0; }
    Lease lease() const { return lease_; }
    Admission acquire(Expected expected, MachineService::Owner current,
                      MachineService::Relation relation, uint64_t epoch,
                      bool owned_stop) {
        if (busy()) return Admission::busy;
        if (!expected.token || expected.token != current.token ||
            relation != MachineService::Relation::Current) return Admission::wrong_owner;
        if (expected.epoch != epoch) return Admission::stale_epoch;
        if (current.kind != MachineService::Kind::ShellExecGuest ||
            current.depth != 2 || !owned_stop) return Admission::not_ready;
        if (generation_ == UINT64_MAX) return Admission::limit;
        lease_ = {expected.token, epoch, ++generation_};
        return Admission::ok;
    }
    bool matches(Lease exact) const {
        return busy() && exact.generation == lease_.generation &&
            exact.token == lease_.token && exact.epoch == lease_.epoch;
    }
    bool release(Lease exact) {
        if (!matches(exact)) return false;
        lease_ = {};
        return true;
    }
};
// Defined by debugger-common. A raw caller cannot borrow the active lease.
Owner &owner();
inline bool &leanMode() { static bool selected = false; return selected; }
inline bool diagnosticEffectAllowed() { return !leanMode() && !owner().busy(); }
inline bool rawEffectAllowed() { return !owner().busy(); }
}
#endif
