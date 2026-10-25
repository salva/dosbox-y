/* GPL-2.0-or-later. Sole Lua worker entry; no emulator globals. */
#ifndef DOSBOX_LUA_DRIVER_WORKER_H
#define DOSBOX_LUA_DRIVER_WORKER_H
#include "lua_driver_options.h"
#include "lua_driver_handoff.h"
namespace LuaDriver {
struct WorkerResult { bool completed=false; std::string error; };
// Run on the Lua-owning thread. Native caller owns and services the handoff
// until isLuaClosed(), then joins without a handoff lock. Result read after join.
void runWorker(const Options &,Handoff &,WorkerResult &);
}
#endif
