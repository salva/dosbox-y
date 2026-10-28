/* GPL-2.0-or-later. Shared GUI-owned task dispatch, no wire parser. */
#ifndef DOSBOX_LIVE_CONTROL_H
#define DOSBOX_LIVE_CONTROL_H
#include <string>
#include "live_request.h"
#include "native_profile.h"
#include "mouse_diagnostic.h"
struct LiveGeometry {
    NativeInput::Profile profile;MouseDiagnostic mouse;
    uint32_t requested_x=0,requested_y=0;NativeInput::Point host;
    bool motion=false;uint32_t dispatch_tick=0;uint64_t generation=0;
};
struct LiveTaskStatus {
    bool configured=false,closed=false,frame_exhausted=false;
    unsigned pending=0,lifetime=0,held=0,frames=0,clock_error=0,cleanup_state=0;
    uint64_t guest_ms=0,epoch=0,cleanup_generation=0,dispatch_sequence=0;
};
LiveTaskStatus GFX_LuaTaskStatus();
bool GFX_CheckpointReady();
// Actual selected windowed SDL surface configuration, distinct from task neutrality.
bool GFX_CheckpointRebase(uint64_t host,uint32_t tick);
LiveGeometry GFX_LuaGeometry();
bool GFX_InstallLuaTasks(int root,size_t limit);
void GFX_PumpLuaTasks(bool running, bool stopped_cleanup=false);
void GFX_FaultLuaTasks();
std::string GFX_ExecuteLuaTask(const NativeInput::Request &request,bool running);
#endif
