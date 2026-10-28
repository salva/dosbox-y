#!/usr/bin/env python3
"""Compile the actual Lua checkpoint projection; native authority is mocked.
This checks adapter refusal/lease/result ordering, not emulator state safety.
"""
import os
from pathlib import Path
import shlex
import subprocess
root = Path(__file__).resolve().parents[2]
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'lua-checkpoint-projection'
w.mkdir(exist_ok=True)
source = (root / 'src/debug/debug_lua.cpp').read_text()
start = source.index('int LiveCheckpoint(lua_State *L) {')
end = source.index('// Pure projection of the one retained target.', start)
preamble = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <stdexcept>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
namespace MachineService {enum class Kind {ShellExecGuest};}
struct Owner {unsigned token=7,depth=2;MachineService::Kind kind=MachineService::Kind::ShellExecGuest;};
static Owner DOSBOX_MachineServiceOwner(){return {};}
static unsigned DOSBOX_MachineServiceRelation(unsigned){return 0;}
namespace NativeControl {
 enum class Admission {ok,denied};
 struct Lease {};
 struct Control {
  bool available=true,held=false;unsigned releases=0;
  Admission acquire(std::pair<unsigned,unsigned>,Owner,unsigned,unsigned,bool) {
   if(!available)return Admission::denied;
   assert(!held);held=true;return Admission::ok;
  }
  Lease lease(){assert(held);return {};}
  void release(Lease){assert(held);held=false;++releases;}
 } control;
 static Control &owner(){return control;}
}
static bool native_admitted=true;
struct Context {unsigned invocation=7,checkpoint=1;};
static Context DEBUG_NativeExecutionContext(){return {};}
namespace NativeDebug {enum class Frontend {Lua};}
struct Attachment {unsigned serial=1;};
static bool DEBUG_AdmitNativeState(Attachment,Context){return native_admitted;}
static bool DEBUG_AdvanceNativeStateContext(Attachment,Context,NativeControl::Lease) {return true;}
static Attachment local_attachment,live_attachment;
static Attachment DEBUG_AttachNativeExecution(NativeDebug::Frontend,unsigned){return {};}
static bool live_active=false;
static bool Debugger_LiveOwnsStop(unsigned){return true;}
static const char *Debugger_LiveStopReason(){return "deliberate";}
static bool Debugger_PredicateHeld(){return false;}
static void PredicateGuard(lua_State *){}
static void LiveCheckpointArguments(lua_State *){}
static bool GFX_CheckpointReady(){return true;}
static bool KEYBOARD_CheckpointNeutral(){return true;}
static bool Mouse_CheckpointNeutral(){return true;}
static bool GFX_CheckpointRebase(unsigned,unsigned){return true;}
static unsigned LiveNow(){return 1;}
static bool Debugger_LiveCheckpointStop(unsigned){return true;}
static int CPU_Core_Normal_Run=0,cpudecoder=0;
struct DebuggerBreakpointInfo {};
static unsigned Debugger_BreakpointList(DebuggerBreakpointInfo *,unsigned,unsigned *,bool){return 0;}
static bool CheckpointInstrumentationAllowed(bool,unsigned,DebuggerBreakpointInfo *,unsigned){return true;}
static std::string CheckpointMounts(){return std::string(1024,'m');}
static std::string RunningProgram="fixture";
static struct {unsigned psp(){return 3;}} dos;
static unsigned PIC_Ticks=4;
static uint64_t checkpoint_owner=0,checkpoint_epoch=0;
static bool checkpoint_restored=false;
class SaveState {
public:
 using Checkpoint=std::map<std::string,std::string>;
 static SaveState &instance(){static SaveState state;return state;}
 bool checkpointSave(Checkpoint &out,size_t){assert(NativeControl::owner().held);out["fixture"]="bytes";return true;}
 bool checkpointLoad(const Checkpoint &){assert(NativeControl::owner().held);return true;}
};
struct LiveCheckpointImage {
 SaveState::Checkpoint parts;uint64_t owner=0;uint16_t psp=0;
 std::string program,mounts;
};
static std::map<std::string,LiveCheckpointImage> live_checkpoints;
[[noreturn]] static void E_Exit(const char *) {throw std::runtime_error("fatal");}
'''
main = r'''
static void invoke(lua_State *L,const char *op,bool expected,const char *error=nullptr) {
 lua_pushcfunction(L,LiveCheckpoint);lua_newtable(L);
 lua_pushstring(L,op);lua_setfield(L,-2,"op");
 lua_pushliteral(L,"fixture");lua_setfield(L,-2,"id");
 assert(lua_pcall(L,1,1,0)==LUA_OK);
 assert(!NativeControl::owner().held);
 lua_getfield(L,-1,"ok");assert(bool(lua_toboolean(L,-1))==expected);lua_pop(L,1);
 lua_getfield(L,-1,"error");
 if(error)assert(lua_type(L,-1)==LUA_TSTRING && !strcmp(lua_tostring(L,-1),error));
 else assert(lua_isnil(L,-1));
 lua_settop(L,0);
}
static bool allocation_failed=false,fail_allocation=false;
static void *allocator(void *,void *p,size_t,size_t size) {
 if(!size){free(p);return nullptr;}
 if(fail_allocation){assert(!NativeControl::owner().held);allocation_failed=true;return nullptr;}
 return realloc(p,size);
}
static int call_with_failure(lua_State *L) {
 fail_allocation=true;
 return LiveCheckpoint(L);
}
int main() {
 lua_State *L=lua_newstate(allocator,nullptr);assert(L);
 native_admitted=false;
 invoke(L,"save",false,"checkpoint native ownership unavailable");
 assert(!NativeControl::owner().releases && live_checkpoints.empty());
 native_admitted=true;NativeControl::owner().available=false;
 invoke(L,"save",false,"checkpoint native ownership unavailable");
 assert(!NativeControl::owner().releases && live_checkpoints.empty());
 NativeControl::owner().available=true;
 invoke(L,"save",true);assert(NativeControl::owner().releases==1);
 invoke(L,"load",true);assert(NativeControl::owner().releases==2 && checkpoint_restored && checkpoint_epoch==1);
 lua_close(L);
 L=lua_newstate(allocator,nullptr);assert(L);
 lua_pushcfunction(L,call_with_failure);lua_newtable(L);
 lua_pushliteral(L,"save");lua_setfield(L,-2,"op");
 lua_pushliteral(L,"fixture");lua_setfield(L,-2,"id");
 // Inject failure only after native save, at allocating its result table.
 // The checkpoint remains retained, and the exact lease is already released.
 live_checkpoints.clear();
 const unsigned releases=NativeControl::owner().releases;
 const int result=lua_pcall(L,1,1,0);
 fail_allocation=false;
 assert(result==LUA_ERRMEM && allocation_failed && !NativeControl::owner().held);
 assert(!live_checkpoints.empty() && live_checkpoints.count("fixture")==1);
 assert(NativeControl::owner().releases==releases+1);
 lua_close(L);
 allocation_failed=false;
 L=lua_newstate(allocator,nullptr);assert(L);
 lua_pushcfunction(L,call_with_failure);lua_newtable(L);
 lua_pushliteral(L,"load");lua_setfield(L,-2,"op");
 lua_pushliteral(L,"fixture");lua_setfield(L,-2,"id");
 const unsigned epoch=checkpoint_epoch;
 const int load_result=lua_pcall(L,1,1,0);
 fail_allocation=false;
 assert(load_result==LUA_ERRMEM && allocation_failed && !NativeControl::owner().held);
 assert(checkpoint_restored && checkpoint_epoch==epoch+1);
 assert(NativeControl::owner().releases==releases+2);
 lua_close(L);
 puts("actual Lua checkpoint projection PASS (native authority mocked)");
}
'''
(w / 'test.cpp').write_text(preamble + source[start:end] + main)
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'lua5.4'], text=True))
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror', str(w / 'test.cpp'), *flags, '-o', str(w / 'test')], check=True)
subprocess.run([str(w / 'test')], check=True)
