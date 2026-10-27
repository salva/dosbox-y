/* GPL-2.0-or-later. Endpoint-owned edges, never confirmed guest state. */
#ifndef DOSBOX_NATIVE_OWNERSHIP_H
#define DOSBOX_NATIVE_OWNERSHIP_H
#include "native_input.h"
namespace NativeInput {
struct Dispatch {
    void (*key)(KBD_KEYS,bool,void *);
    void (*button)(unsigned,bool,void *);
    void *context;
};
enum class CleanupState { Idle, Pending, Deferred, Dispatched, Failed };
struct Cleanup {
    uint64_t generation=0;
    CleanupState state=CleanupState::Idle;
    unsigned attempted[EdgeCount]={};
    unsigned attempts=0;
    bool remaining[EdgeCount]={};
    uint64_t host_ms=0, guest_ms=0;
};
class Ownership {
    bool held_[EdgeCount]={};
    uint64_t host_deadline_[EdgeCount]={},guest_deadline_[EdgeCount]={};
    Cleanup cleanup_;
    bool exhausted_=false;
    static KBD_KEYS ordinal(unsigned index) {
        return KeyVocabulary[index].value;
    }
    static void call(unsigned index,bool down,Dispatch dispatch) {
        if (index<KeyCount) dispatch.key(ordinal(index),down,dispatch.context);
        else dispatch.button(index-KeyCount,down,dispatch.context);
    }
    void snapshot() {for (unsigned i=0;i<EdgeCount;i++) cleanup_.remaining[i]=held_[i];}
public:
    bool held(unsigned index) const {return index<EdgeCount && held_[index];}
    bool any() const {for (bool h:held_) if(h) return true;return false;}
    bool blocked() const {return exhausted_ || cleanup_.state==CleanupState::Pending ||
        cleanup_.state==CleanupState::Deferred || cleanup_.state==CleanupState::Failed;}
    bool exhausted() const {return exhausted_;}
    const Cleanup &receipt() const {return cleanup_;}
    // Validation includes lease-counter safety BEFORE any primitive call.
    bool edge(unsigned index,bool down,uint64_t host,uint64_t guest,Dispatch dispatch) {
        if (index>=EdgeCount || blocked() || held_[index]==down ||
            !dispatch.key || !dispatch.button ||
            (down && (host>UINT64_MAX-2000 || guest>UINT64_MAX-1000))) return false;
        try {call(index,down,dispatch);} catch (...) {return false;}
        held_[index]=down; // ONLY normal primitive return establishes ownership
        if (down) {host_deadline_[index]=host+2000;guest_deadline_[index]=guest+1000;}
        snapshot();return true;
    }
    bool expired(uint64_t host,uint64_t guest) const {
        for (unsigned i=0;i<EdgeCount;i++) if (held_[i] &&
            (host>=host_deadline_[i] || guest>=guest_deadline_[i])) return true;
        return false;
    }
    // Queue owner must invalidate ALL pending mutations/epoch before requesting
    // cleanup. This primitive does not fabricate command terminal receipts.
    bool request(uint64_t host,uint64_t guest,bool force=false) {
        if (!force && !any() && !blocked()) return false; // repeated empty release, no generation
        if (cleanup_.state==CleanupState::Pending || cleanup_.state==CleanupState::Deferred ||
            cleanup_.state==CleanupState::Failed) return false;
        if (cleanup_.generation==UINT64_MAX) {exhausted_=true;return false;}
        ++cleanup_.generation;cleanup_.state=CleanupState::Pending;
        cleanup_.attempts=0;cleanup_.host_ms=host;cleanup_.guest_ms=guest;snapshot();return true;
    }
    void clean(bool running,uint64_t host,uint64_t guest,Dispatch dispatch) {
        if (cleanup_.state!=CleanupState::Pending && cleanup_.state!=CleanupState::Deferred) return;
        cleanup_.host_ms=host;cleanup_.guest_ms=guest;
        if (!running || !dispatch.key || !dispatch.button) {cleanup_.state=CleanupState::Deferred;return;}
        // Sorted actual enum ordinals, not public vocabulary declaration order.
        unsigned order[EdgeCount];for(unsigned i=0;i<EdgeCount;i++) order[i]=i;
        for(unsigned i=0;i<KeyCount;i++) for(unsigned j=i+1;j<KeyCount;j++)
            if(ordinal(order[j])<ordinal(order[i])) {unsigned t=order[i];order[i]=order[j];order[j]=t;}
        for(unsigned index:order) if(held_[index]) {
            // Record invocation even if primitive throws. No retry of an uncertain
            // failed release; retain H, failed blocks data for owner reconciliation.
            cleanup_.attempted[cleanup_.attempts++]=index;
            try {call(index,false,dispatch);} catch (...) {cleanup_.state=CleanupState::Failed;snapshot();return;}
            held_[index]=false;snapshot();
        }
        cleanup_.state=CleanupState::Dispatched;
    }
};
}
#endif
