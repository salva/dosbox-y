#include "../../include/machine_service.h"
#include <stdexcept>
#include <stdio.h>
using namespace MachineService;
struct Context {Coordinator *owner;unsigned calls=0;bool fail=false;};
static void slice(void *opaque) {
    auto &context=*static_cast<Context *>(opaque);auto &owner=*context.owner;
    assert(owner.active());++context.calls;
    const auto prior_denial=owner.denial();
    assert(owner.admission()==Denial::Active);
    assert(owner.frontierAdmission()==Denial::Active);
    assert(owner.denial()==prior_denial);
    const auto token=owner.current().token;
    owner.request();assert(!owner.service(slice,opaque));
    {Coordinator::Scope child(owner);assert(!owner.service(slice,opaque));}
    assert(owner.current_token(token));
    if(context.fail)throw std::runtime_error("service");
}
static uint64_t departing=0;
static unsigned departures=0;
static void departed(uint64_t token) {departing=token;++departures;}
int main() {
    // Local classification is not a service/state waiver.
    for (const auto kind : {Kind::CallbackGuest,Kind::IdleGuest,
                            Kind::FaultGuest,Kind::DiskGuest}) {
        Coordinator c;
        assert(c.frontierAdmission()==Denial::NoOwner);
        Coordinator::Scope root(c,Kind::RootGuest);
        const auto parent=root.token();
        {
            Coordinator::Scope nested(c,kind);
            assert(c.frontierAdmission()==Denial::None);
            assert(c.contextAdmission()==Denial::UnknownAncestor);
            assert(c.relation(parent)==Relation::Suspended);
            assert(!c.requestForCurrent(nested.token(),true));
            Coordinator::Scope unknown(c);
            assert(c.frontierAdmission()==Denial::UnknownOwner);
            Coordinator::Scope marked(c,Kind::CallbackGuest);
            assert(c.frontierAdmission()==Denial::UnknownAncestor);
        }
        assert(c.frontierAdmission()==Denial::None);
        assert(c.contextAdmission()==Denial::None);
        assert(c.relation(parent)==Relation::Current);
    }
    Coordinator lifetime;
    uint64_t parent=0,child_token=0;
    {
        Coordinator::Scope root(lifetime,Kind::RootGuest);
        {
            Coordinator::Scope arm(lifetime,Kind::ShellExecGuest,departed);
            parent=arm.token();
            assert(lifetime.relation(parent)==Relation::Current);
            try {
                Coordinator::Scope child(lifetime,Kind::Unclassified,departed);
                child_token=child.token();
                assert(lifetime.relation(parent)==Relation::Suspended);
                assert(lifetime.relation(child_token)==Relation::Current);
                assert(lifetime.admission()==Denial::UnknownOwner);
                throw std::runtime_error("child");
            } catch(const std::runtime_error &) {}
            assert(departing==child_token && departures==1);
            assert(lifetime.relation(parent)==Relation::Current);
            assert(lifetime.relation(child_token)==Relation::Absent);
        }
        assert(departing==parent && departures==2);
        Coordinator::Scope sibling(lifetime,Kind::ShellExecGuest);
        assert(lifetime.relation(parent)==Relation::Absent);
        assert(sibling.token()!=parent);
    }
    assert(lifetime.relation(parent)==Relation::Absent);

    Coordinator owner;Context context{&owner};owner.request();
    assert(!owner.service(slice,&context));
    assert(owner.admission()==Denial::NoOwner);
    uint64_t retired=0;
    {
        Coordinator::Scope root(owner,Kind::RootGuest);
        assert(owner.admission()==Denial::None && owner.pending());
        assert(owner.denial()==Denial::NoOwner);
        assert(owner.service(slice,&context));
        {
            Coordinator::Scope exec(owner,Kind::ShellExecGuest);retired=exec.token();
            {Coordinator::Scope child(owner);assert(!owner.service(slice,&context));
             assert(owner.denial()==Denial::UnknownOwner && owner.pending());
             const auto before=owner.current();
             assert(owner.admission()==Denial::UnknownOwner);
             assert(owner.pending() && !owner.active() && owner.current_token(before.token));}
            assert(owner.admission()==Denial::None);
            assert(owner.denial()==Denial::UnknownOwner); // probe did not publish
            assert(owner.service(slice,&context));
            context.fail=true;
            try {owner.service(slice,&context);assert(false);}catch(const std::runtime_error &){}
            assert(!owner.active() && owner.pending());context.fail=false;
        }
        assert(!owner.current_token(retired));
        assert(owner.service(slice,&context));
    }
    assert(owner.depth()==0 && !owner.current_token(retired));
    {Coordinator::Scope unknown(owner);Coordinator::Scope false_depth2(owner,Kind::ShellExecGuest);
     assert(!owner.service(slice,&context) && owner.denial()==Denial::UnknownAncestor);}
    {Coordinator::Scope root(owner,Kind::RootGuest);Coordinator::Scope false_root(owner,Kind::RootGuest);
     assert(!owner.service(slice,&context));}
    {Coordinator::Scope root(owner,Kind::RootGuest);Coordinator::Scope unknown(owner);
     Coordinator::Scope exec(owner,Kind::ShellExecGuest);
     assert(!owner.service(slice,&context) && owner.denial()==Denial::UnknownAncestor);}
    auto deep=[](Coordinator &c,unsigned n,auto &self)->void {
        if(!n) {assert(c.relation(1)==Relation::Indeterminate);assert(c.admission()==Denial::Overflow);assert(!c.eligible() && c.denial()==Denial::Overflow);return;}
        Coordinator::Scope entry(c,n==17?Kind::RootGuest:Kind::ShellExecGuest);
        self(c,n-1,self);
    };
    deep(owner,17,deep);assert(owner.depth()==0);
    Coordinator exhausted(UINT64_MAX);Coordinator::Scope entry(exhausted,Kind::RootGuest);
    assert(exhausted.relation(1)==Relation::Indeterminate);exhausted.request();assert(exhausted.admission()==Denial::TokenExhausted);assert(!exhausted.service(slice,&context));
    assert(exhausted.denial()==Denial::TokenExhausted);
    puts("read-only admission/classified chain/unmarked child/reentry/exception/overflow/stale-token PASS (unit)");
}
