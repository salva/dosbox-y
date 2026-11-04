#include "native_control_owner.h"
#include <cassert>
using namespace MachineService;
using namespace NativeControl;
int main() {
    Coordinator machine;
    NativeControl::Owner control;
    Coordinator::Scope root(machine,Kind::RootGuest);
    const auto acquire=[&](uint64_t restored=0) {
        const auto current=machine.current();
        return control.acquire({current.token,9},current,
            machine.relation(current.token),9,true,
            machine.contextAdmission(),restored);
    };
    assert(acquire()==Admission::not_ready);
    assert(acquire(root.token()+1)==Admission::not_ready);
    assert(acquire(root.token())==Admission::ok);
    const auto exact=control.lease();
    assert(acquire(root.token())==Admission::busy);
    assert(!control.release({exact.token,exact.epoch,exact.generation+1}));
    assert(control.release(exact));
    {
        Coordinator::Scope child(machine,Kind::ShellExecGuest);
        assert(acquire()==Admission::ok);
        assert(control.release(control.lease()));
        {
            Coordinator::Scope grandchild(machine,Kind::ShellExecGuest);
            // Legitimate descendants are not fixed to shell depth two.
            assert(acquire()==Admission::ok);
            assert(control.release(control.lease()));
        }
        {
            Coordinator::Scope callback(machine,Kind::CallbackGuest);
            Coordinator::Scope nested(machine,Kind::ShellExecGuest);
            assert(acquire()==Admission::not_ready);
        }
    }
    {
        Coordinator::Scope unknown(machine);
        Coordinator::Scope child(machine,Kind::ShellExecGuest);
        assert(acquire()==Admission::not_ready);
    }
    assert(control.acquire({root.token(),8},machine.current(),Relation::Current,
                           9,true,Denial::None,root.token())==Admission::stale_epoch);
    assert(control.acquire({root.token()+1,9},machine.current(),Relation::Current,
                           9,true,Denial::None,root.token())==Admission::wrong_owner);
    assert(control.acquire({root.token(),9},machine.current(),Relation::Current,
                           9,false,Denial::None,root.token())==Admission::not_ready);
}
