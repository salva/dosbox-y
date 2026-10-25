/* GPL-2.0-or-later. Classified single-thread machine service admission.
 * Event/CPU/render callbacks request work, never invoke the service.
 */
#ifndef DOSBOX_MACHINE_SERVICE_H
#define DOSBOX_MACHINE_SERVICE_H
#include <assert.h>
#include <stdint.h>
#include <limits.h>
namespace MachineService {
enum class Kind { Unclassified, RootGuest, ShellExecGuest,
                  CallbackGuest, IdleGuest, FaultGuest, DiskGuest };
enum class Relation { Current, Suspended, Absent, Indeterminate };
enum class Denial { None, NoOwner, UnknownOwner, UnknownAncestor, Overflow, Active, TokenExhausted };
struct Owner { Kind kind=Kind::Unclassified;uint64_t token=0;unsigned depth=0; };
class Coordinator {
    static constexpr unsigned Capacity=16;
    Owner stack_[Capacity];
    unsigned depth_=0;
    uint64_t next_=0;
    bool exhausted_=false,pending_=false,active_=false;
    Denial denial_=Denial::NoOwner;
    uint64_t enter(Kind kind) {
        assert(depth_!=UINT_MAX);
        ++depth_;
        if(next_==UINT64_MAX)exhausted_=true;
        const uint64_t token=exhausted_?0:++next_;
        if(depth_<=Capacity)stack_[depth_-1]={kind,token,depth_};
        return token;
    }
public:
    explicit Coordinator(uint64_t last_token=0):next_(last_token) {}
    class Scope {
        Coordinator &owner_;
        uint64_t token_;
        void (*depart_)(uint64_t);
    public:
        explicit Scope(Coordinator &owner,Kind kind=Kind::Unclassified,
                       void (*depart)(uint64_t)=nullptr)
            :owner_(owner),token_(owner.enter(kind)),depart_(depart) {}
        ~Scope() {assert(owner_.depth_);if(depart_)depart_(token_);if(owner_.depth_<=Capacity)owner_.stack_[owner_.depth_-1]={};--owner_.depth_;}
        uint64_t token() const {return token_;}
        Scope(const Scope &)=delete;
        Scope &operator=(const Scope &)=delete;
    };
    void request() {pending_=true;}
    // Checked enqueue for a job prepared by this exact classified invocation.
    // May be called inside its service callback; it does not execute service.
    bool requestForCurrent(uint64_t token, bool registered) {
        if(!registered || !token || exhausted_ || !depth_ || depth_>Capacity ||
           current().token!=token)return false;
        for(unsigned i=0;i<depth_;++i)
            if(stack_[i].kind!=(i?Kind::ShellExecGuest:Kind::RootGuest))return false;
        pending_=true;return true;
    }
    bool pending() const {return pending_;}
    bool active() const {return active_;}
    unsigned depth() const {return depth_;}
    Owner current() const {return depth_ && depth_<=Capacity?stack_[depth_-1]:Owner{};}
    bool current_token(uint64_t token) const {return token && current().token==token;}
    // Exact membership, not numeric ordering or kind/depth inference. An
    // unrepresented current frame makes all membership queries fail closed.
    Relation relation(uint64_t token) const {
        if(exhausted_ || depth_>Capacity)return Relation::Indeterminate;
        if(!token)return Relation::Absent;
        for(unsigned i=0;i<depth_;++i)
            if(stack_[i].token==token)
                return i+1==depth_?Relation::Current:Relation::Suspended;
        return Relation::Absent;
    }
    Denial denial() const {return denial_;}
    // Audited grammar: one RootGuest, then zero or more ShellExecGuest owners.
    // Numeric depth alone grants nothing; any unclassified ancestor denies.
    // Observe admission at an exact native hit without requesting service or
    // changing the retained denial from the last real service attempt.
    Denial contextAdmission() const {
        if(exhausted_)return Denial::TokenExhausted;
        if(!depth_)return Denial::NoOwner;
        if(depth_>Capacity)return Denial::Overflow;
        if(current().kind==Kind::Unclassified)return Denial::UnknownOwner;
        for(unsigned i=0;i<depth_;i++) {
            const Kind required=i==0?Kind::RootGuest:Kind::ShellExecGuest;
            if(stack_[i].kind!=required)return Denial::UnknownAncestor;
        }
        return Denial::None;
    }
    // Local execution frontiers may retain known native continuations. This
    // does not widen ordinary service/state admission (Root -> ShellExec).
    Denial frontierAdmission() const {
        if(active_)return Denial::Active;
        if(exhausted_)return Denial::TokenExhausted;
        if(!depth_)return Denial::NoOwner;
        if(depth_>Capacity)return Denial::Overflow;
        if(current().kind==Kind::Unclassified)return Denial::UnknownOwner;
        if(stack_[0].kind!=Kind::RootGuest)return Denial::UnknownAncestor;
        for(unsigned i=1;i<depth_;++i) {
            switch(stack_[i].kind) {
            case Kind::ShellExecGuest: case Kind::CallbackGuest:
            case Kind::IdleGuest: case Kind::FaultGuest: case Kind::DiskGuest:
                break;
            default:return Denial::UnknownAncestor;
            }
        }
        return Denial::None;
    }
    Denial admission() const {
        return active_?Denial::Active:contextAdmission();
    }
    bool eligible() {
        denial_=admission();
        return denial_==Denial::None;
    }
    // Call only after current handler returned zero. CPU and current callback
    // are inactive; audited ancestors may remain suspended continuations.
    bool service(void (*slice)(void *),void *context) {
        if(!pending_ || !eligible() || !slice)return false;
        pending_=false;active_=true;
        struct Exit {bool &active;~Exit(){active=false;}} exit{active_};
        slice(context);return true;
    }
};
}
// Emulator-thread only. Registration is debugger-owned, requests data-only.
void DOSBOX_SetMachineService(void (*slice)(void *),void *context);
void DOSBOX_RequestMachineService();
bool DOSBOX_EnqueueMachineService(uint64_t token);
MachineService::Owner DOSBOX_MachineServiceOwner();
MachineService::Denial DOSBOX_MachineServiceAdmission();
MachineService::Denial DOSBOX_MachineContextAdmission();
MachineService::Denial DOSBOX_MachineFrontierAdmission();
MachineService::Relation DOSBOX_MachineServiceRelation(uint64_t token);
#endif
