#include "native_frontier.h"
#include <cassert>
#include <stdexcept>
#include <type_traits>
using namespace NativeFrontier;
static_assert(!std::is_copy_constructible<Frontiers>::value,"frontier owner is unique");
static_assert(!std::is_copy_assignable<Frontiers>::value,"frontier owner is unique");
struct Owner {Frontiers frontier;unsigned calls=0;Kind expected=Kind::NestedEntry;bool decoder=false,callback=false,fail=false;};
static void hold(void *opaque,const Descriptor &descriptor) {
    auto &owner=*static_cast<Owner *>(opaque);++owner.calls;
    assert(owner.frontier.holding());
    assert(owner.frontier.ownsHold(descriptor));
    const auto copy=descriptor;
    assert(!owner.frontier.ownsHold(copy));
    assert(descriptor.ordinal==owner.calls);
    assert(descriptor.kind==owner.expected && descriptor.invocation==7);
    assert(descriptor.decoder_entered==owner.decoder && descriptor.callback_entered==owner.callback);
    // Held service may not reenter the frontier or replace its owner.
    // Holding authority is exact descriptor identity, not equal copied facts.
    owner.frontier.reach(Kind::LoopStop,7);
    assert(owner.frontier.holding());
    assert(owner.frontier.ownsHold(descriptor));
    assert(!owner.frontier.unbind(hold,&owner));
    if(owner.fail)throw std::runtime_error("native failure");
}
int main() {
    Owner owner;assert(owner.frontier.bind(hold,&owner));
    const Descriptor outside{Kind::NestedEntry,7,false,false,1};
    assert(!owner.frontier.holding());
    assert(!owner.frontier.ownsHold(outside));
    assert(!owner.frontier.bind(hold,&owner));
    owner.frontier.reach(Kind::NestedEntry,0);assert(!owner.calls);
    owner.frontier.reach(Kind::NestedEntry,7);
    {
        // Known ShellExec ancestry adds no extra callback scope.
        Frontiers::Scope shell(owner.frontier,false,false);
        owner.frontier.reach(Kind::NestedEntry,7);
    }
    {
        Frontiers::Scope callback(owner.frontier,false);owner.callback=true;
        // It also cannot hide a real enclosing callback.
        Frontiers::Scope shell(owner.frontier,false,false);
        owner.frontier.reach(Kind::NestedEntry,7);
        {
            Frontiers::Scope decoder(owner.frontier,true);owner.decoder=true;
            owner.expected=Kind::PartialParent;owner.frontier.reach(Kind::PartialParent,7);
        }
        owner.decoder=false;owner.expected=Kind::NativeReturn;
        owner.frontier.reach(Kind::PartialParent,7);
    }
    owner.callback=false;owner.fail=true;owner.expected=Kind::ExitFence;
    try {owner.frontier.reach(Kind::ExitFence,7);assert(false);}catch(const std::runtime_error &){}
    assert(!owner.frontier.holding());
    assert(!owner.frontier.ownsHold(outside));
    owner.fail=false;owner.frontier.reach(Kind::ExitFence,7);
    assert(owner.calls==7 && owner.frontier.unbind(hold,&owner));
}
