/* GPL-2.0-or-later. Native lifetime of one disposable restored DOS invocation.
 * The startup owner validates the installed PSP/SDA/MCB before selecting it;
 * no guest PSP switch or frontend attachment can change the selected root.
 */
#ifndef DOSBOX_RESTORED_SESSION_H
#define DOSBOX_RESTORED_SESSION_H
#include <cstdint>
namespace RestoredSession {
struct Exit {
    uint64_t invocation;
    uint16_t root;
    uint8_t code;
    bool resident, self_parent;
};
// Deliberately not a guest fault, integer reboot, or std::exception error.
struct RootExit { Exit facts; };
class Invocation {
    uint64_t invocation_=0;
    uint16_t root_=0;
    bool terminal_=false;
    Exit exit_{};
public:
    // One-shot selection, after startup installation/ownership validation.
    // A terminal invocation cannot be rearmed, including by a finalizer.
    bool select(uint64_t invocation,uint16_t root) {
        if(invocation_ || !invocation || !root)return false;
        invocation_=invocation;root_=root;return true;
    }
    bool selectedRoot(uint16_t psp) const {return invocation_ && psp==root_;}
    bool selected() const {return invocation_!=0;}
    bool owns(uint64_t invocation) const {return invocation_ && invocation_==invocation;}
    bool terminal() const {return terminal_;}
    const Exit &exit() const {return exit_;}
    void terminate(uint16_t psp,bool resident,uint8_t code,bool self_parent) {
        if(!selectedRoot(psp))return;
        if(!terminal_) {
            exit_={invocation_,root_,code,resident,self_parent};
            terminal_=true;
        }
        throw RootExit{exit_};
    }
};
// Emulator thread only. Fresh startup owns selection and catches outside guest
// frames. Normal sessions leave it unselected. No reset/reuse/live-load API.
Invocation &invocation();
// Owning DOS process check; does not select an invocation or grant execution.
bool validRoot(uint16_t root, bool capture);
bool selected();
}
#endif
