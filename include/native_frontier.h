/* GPL-2.0-or-later. Emulator-thread native continuation seams.
 * Descriptors are borrowed only for the duration of their entered hook. This
 * facility neither grants admission nor invokes Coordinator::service.
 */
#ifndef DOSBOX_NATIVE_FRONTIER_H
#define DOSBOX_NATIVE_FRONTIER_H
#include <cassert>
#include <cstdint>
#include <stdexcept>
namespace NativeFrontier {
enum class Kind { Predispatch, PostTick, Decoder, CallbackEntry, NestedEntry, NativeReturn, PartialParent,
                  LoopStop, ExitFence };
struct Descriptor {
    Kind kind;
    uint64_t invocation;
    bool decoder_entered, callback_entered;
    uint64_t ordinal;
};
class Frontiers {
    unsigned decoders_=0, callbacks_=0;
    using Hook=void (*)(void *,const Descriptor &);
    Hook hook_=nullptr;
    void *owner_=nullptr;
    const Descriptor *held_=nullptr;
    uint64_t ordinal_=0;
public:
    Frontiers()=default;
    Frontiers(const Frontiers &)=delete;
    Frontiers &operator=(const Frontiers &)=delete;
    // Authority is the currently entered call-local descriptor, not merely a
    // copied ordinal or a transport phase. It expires on normal/exception exit.
    bool ownsHold(const Descriptor &descriptor) const {return held_==&descriptor;}
    bool holding() const {return held_!=nullptr;}
    // One selected local adapter, on the existing native thread only. Binding
    // does not widen native stack, lease or execution-slot admission.
    bool bind(Hook hook,void *owner) {
        if(!hook || hook_ || held_)return false;
        hook_=hook;owner_=owner;return true;
    }
    bool unbind(Hook hook,void *owner) {
        if(held_ || hook_!=hook || owner_!=owner || !hook)return false;
        hook_=nullptr;owner_=nullptr;return true;
    }
    // decoder=true tracks an entered interpreter. false tracks a native
    // callback or named wrapper suffix, including its setup/restoration. Such
    // frames restrict E/S even when no outer interpreter is entered.
    class Scope {
        unsigned *depth_;
    public:
        Scope(Frontiers &frontiers,bool decoder,bool enabled=true)
            :depth_(enabled?&(decoder?frontiers.decoders_:frontiers.callbacks_):nullptr) {
            if(depth_)++*depth_;
        }
        ~Scope(){if(depth_){assert(*depth_);--*depth_;}}
        Scope(const Scope &)=delete;
        Scope &operator=(const Scope &)=delete;
    };
    void reach(Kind kind,uint64_t invocation) {
        if(!hook_ || held_ || !invocation)return;
        if(kind==Kind::PartialParent && !decoders_)kind=Kind::NativeReturn;
        // Exhaustion must not reuse an earlier boundary identity.
        if(ordinal_==UINT64_MAX)throw std::overflow_error("native frontier identity exhausted");
        const Descriptor descriptor{kind,invocation,decoders_!=0,callbacks_!=0,++ordinal_};
        held_=&descriptor;
        struct Restore {
            const Descriptor *&held;
            ~Restore(){held=nullptr;}
        } restore{held_};
        hook_(owner_,descriptor);
    }
};
inline Frontiers &frontiers(){static Frontiers instance;return instance;}
}
#endif
