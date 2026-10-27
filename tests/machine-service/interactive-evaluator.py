#!/usr/bin/env python3
"""Compile actual persistent evaluator/sink against Lua; native admission is a fixture.
This does not compile the production evaluator or qualify native execution.
"""
import os
from pathlib import Path
import shlex
import subprocess
root = Path(__file__).resolve().parents[2]
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'lua-interactive-check'
w.mkdir(exist_ok=True)
source = (root/'src/debug/debug_lua.cpp').read_text()
def section(start, end):
    return source[source.index(start):source.index(end, source.index(start))]
# Keep extraction boundaries explicit so the test compiles actual owning functions,
# rather than a manually rewritten evaluator with independent behavior.
preamble = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <exception>
#include <cstdlib>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
namespace NativeDebug {struct Attachment {unsigned serial=0;};enum class Frontend {Interactive};}
static bool admitted=true;
static bool DEBUG_AdmitNativeEffect(NativeDebug::Attachment a) {return admitted && a.serial==1;}
static NativeDebug::Attachment DEBUG_AttachNativeExecution(NativeDebug::Frontend,unsigned) {return {1};}
static std::string output,log_output;
static void append(std::string &sink,const char *fmt,va_list args) {char bytes[4096];vsnprintf(bytes,sizeof(bytes),fmt,args);sink+=bytes;}
static void DEBUG_ShowMsg(const char *fmt,...) {va_list args;va_start(args,fmt);append(output,fmt,args);va_end(args);}
static void LOG_MSG(const char *fmt,...) {va_list args;va_start(args,fmt);append(log_output,fmt,args);va_end(args);}
static lua_State *interactive_lua=nullptr,*live_lua=nullptr,*predicate_state=nullptr;
static bool interactive_active=false,local_evaluation=false,predicate_active=false,predicate_closing=false;
static NativeDebug::Attachment local_attachment,interactive_attachment;
static void PredicateInstallLoadGuards(lua_State *) {}
bool Debugger_LuaInteractiveReset();
'''
# Both adjacent log functions end at the predicate namespace.
logs = source[source.index('static int Debugger_LuaLog(lua_State *L)'):]
end = logs.index('\nnamespace {', logs.index('static int Debugger_LuaLogf')+1)
logs = logs[:end]
register = r'''
static void Debugger_LuaRegisterDbg(lua_State *L) {
 lua_newtable(L);
 lua_pushcfunction(L,Debugger_LuaLog);lua_setfield(L,-2,"log");
 lua_pushcfunction(L,Debugger_LuaLogf);lua_setfield(L,-2,"logf");
 lua_setglobal(L,"dbg");
}
'''
initialize_fixture = r'''
namespace NativeControl {static bool leanMode(){return false;}}
static const char *live_module="dispatcher.lua";
static int unused_live_operation(lua_State *) {return 0;}
#define LiveExecutableEntry unused_live_operation
#define LiveExecutionTarget unused_live_operation
#define LiveCheckpoint unused_live_operation
#define LiveStateFile unused_live_operation
#define LivePhase unused_live_operation
#define LiveTask unused_live_operation
#define LiveStatus unused_live_operation
#define LiveMouseWitnessArm unused_live_operation
#define LiveStop unused_live_operation
#define LiveResume unused_live_operation
static void LeanRegister(lua_State *) {}
static int LiveDispatchProtected(lua_State *) {return 0;}
static lua_State *live_dispatch_thread=nullptr;
static int live_dispatch_ref=LUA_NOREF;
static int predicate_ref=LUA_NOREF,predicate_id=7,predicate_hits=2,predicate_cs=1,predicate_ip=2;
struct Summary {unsigned sequence=3;};
static Summary Debugger_PredicateSummary(){return {};}
static unsigned Debugger_PredicateTick(){return 4;}
static bool Debugger_PredicateTasksUnchanged(){return true;}
static int Debugger_PredicateRegisters(){return 0;}
static void Debugger_LuaPushRegisters(lua_State *L,int){lua_newtable(L);}
'''
main = r'''
static bool fail_allocation=false;
static void *allocator(void *,void *p,size_t,size_t size) {
 if(!size) {free(p);return nullptr;}
 if(fail_allocation)return nullptr;
 return realloc(p,size);
}
static int check_live_string(lua_State *L) {LiveString(L,1);return 0;}
static int check_live_fields(lua_State *L) {
 static const char *fields[]={"op","id",nullptr};LiveFields(L,1,fields);return 0;
}
static int check_checkpoint_arguments(lua_State *L) {LiveCheckpointArguments(L);return 0;}
int main() {
 lua_State *strings=luaL_newstate();assert(strings);
 lua_pushcfunction(strings,check_live_string);lua_pushstring(strings,"poll");
 assert(lua_pcall(strings,1,0,0)==LUA_OK);
 lua_pushcfunction(strings,check_live_string);lua_pushlstring(strings,"poll\0suffix",11);
 assert(lua_pcall(strings,1,0,0)!=LUA_OK);lua_settop(strings,0);
 lua_pushcfunction(strings,check_live_string);lua_pushinteger(strings,42);
 assert(lua_pcall(strings,1,0,0)!=LUA_OK);lua_settop(strings,0);
 lua_pushcfunction(strings,check_live_fields);lua_newtable(strings);
 lua_pushlstring(strings,"op\0suffix",9);lua_pushstring(strings,"save");lua_rawset(strings,-3);
 assert(lua_pcall(strings,1,0,0)!=LUA_OK);lua_settop(strings,0);
 lua_pushcfunction(strings,check_live_fields);lua_newtable(strings);
 lua_pushinteger(strings,7);lua_pushstring(strings,"save");lua_rawset(strings,-3);
 assert(lua_pcall(strings,1,0,0)!=LUA_OK);lua_settop(strings,0);
 lua_pushcfunction(strings,check_live_fields);lua_newtable(strings);
 lua_pushstring(strings,"save");lua_setfield(strings,-2,"op");
 lua_pushstring(strings,"sample");lua_setfield(strings,-2,"id");
 assert(lua_pcall(strings,1,0,0)==LUA_OK);
 for(const char *request:{"return {op='save', id='sample'}", "return {op='save',id='sample', ['op'..string.char(0)..'suffix']='load'}", "return {op='save',id='sample'..string.char(0)}", "return setmetatable({op='save',id='sample'}, {})", "return {op=42,id='sample'}", "return {op='inspect',id='sample'}", "return {op='save',id=''}", "return {op='save',id='../escape'}", "return {op='save',id=string.rep('a',65)}"}) {
  luaL_openlibs(strings);assert(luaL_loadstring(strings,request)==LUA_OK);
  assert(lua_pcall(strings,0,1,0)==LUA_OK);
  lua_pushcfunction(strings,check_checkpoint_arguments);lua_insert(strings,-2);
  const int result=lua_pcall(strings,1,0,0);
  if(!strcmp(request,"return {op='save', id='sample'}"))assert(result==LUA_OK);
  else assert(result!=LUA_OK);
  lua_settop(strings,0);
 }
 lua_close(strings);
 lua_State *init=luaL_newstate();assert(init);
 lua_pushcfunction(init,LivePrepareDispatch);assert(lua_pcall(init,0,0,0)==LUA_OK);
 assert(live_dispatch_thread && live_dispatch_ref!=LUA_NOREF);
 int dispatched=0;assert(lua_resume(live_dispatch_thread,init,0,&dispatched)==LUA_OK);
 luaL_unref(init,LUA_REGISTRYINDEX,live_dispatch_ref);live_dispatch_ref=LUA_NOREF;live_dispatch_thread=nullptr;
 lua_pushcfunction(init,LiveInitialize);assert(lua_pcall(init,0,0,0)==LUA_OK);
 lua_pushcfunction(init,LiveInitializeLean);assert(lua_pcall(init,0,0,0)!=LUA_OK);lua_settop(init,0);
 live_module="lean-dispatcher.lua";lua_pushcfunction(init,LiveInitializeLean);
 assert(lua_pcall(init,0,0,0)==LUA_OK);
 live_module="missing.lua";lua_pushcfunction(init,LiveInitialize);
 assert(lua_pcall(init,0,0,0)!=LUA_OK);lua_settop(init,0);
 live_module="bad-dispatcher.lua";lua_pushcfunction(init,LiveInitialize);
 assert(lua_pcall(init,0,0,0)!=LUA_OK);lua_settop(init,0);
 assert(luaL_loadstring(init,"return function(event) assert(event.id==7 and event.regs and event.sequence=='3'); return false end")==LUA_OK);
 assert(lua_pcall(init,0,1,0)==LUA_OK);predicate_ref=luaL_ref(init,LUA_REGISTRYINDEX);
 lua_pushcfunction(init,LivePredicateInvoke);assert(lua_pcall(init,0,1,0)==LUA_OK);
 assert(lua_isboolean(init,-1) && !lua_toboolean(init,-1));lua_settop(init,0);
 assert(luaL_loadstring(init,"return function() error(42) end")==LUA_OK);
 assert(lua_pcall(init,0,1,0)==LUA_OK);luaL_unref(init,LUA_REGISTRYINDEX,predicate_ref);predicate_ref=luaL_ref(init,LUA_REGISTRYINDEX);
 lua_pushcfunction(init,LivePredicateInvoke);assert(lua_pcall(init,0,1,0)!=LUA_OK);
 assert(lua_type(init,-1)==LUA_TNUMBER);lua_close(init);
 lua_State *dispatch_alloc=lua_newstate(allocator,nullptr);assert(dispatch_alloc);
 lua_pushcfunction(dispatch_alloc,LivePrepareDispatch);
 fail_allocation=true;assert(lua_pcall(dispatch_alloc,0,0,0)!=LUA_OK);
 fail_allocation=false;assert(!live_dispatch_thread && live_dispatch_ref==LUA_NOREF);lua_close(dispatch_alloc);
 lua_State *initialize_alloc=lua_newstate(allocator,nullptr);assert(initialize_alloc);
 lua_pushcfunction(initialize_alloc,LiveInitialize);
 fail_allocation=true;assert(lua_pcall(initialize_alloc,0,0,0)!=LUA_OK);
 fail_allocation=false;lua_close(initialize_alloc);
 lua_State *predicate_alloc=lua_newstate(allocator,nullptr);assert(predicate_alloc);
 lua_pushcfunction(predicate_alloc,unused_live_operation);predicate_ref=luaL_ref(predicate_alloc,LUA_REGISTRYINDEX);
 lua_pushcfunction(predicate_alloc,LivePredicateInvoke);
 fail_allocation=true;assert(lua_pcall(predicate_alloc,0,1,0)!=LUA_OK);
 fail_allocation=false;lua_close(predicate_alloc);
 const NativeDebug::Attachment a{1};
 interactive_lua=lua_newstate(allocator,nullptr);assert(interactive_lua);
 // Initialization allocates under the protected function too.
 lua_pushcfunction(interactive_lua,InteractiveInitialize);
 assert(lua_pcall(interactive_lua,0,0,0)==LUA_OK);
 interactive_attachment=a;
 assert(Debugger_LuaInteractive("x=40",false,a));
 assert(Debugger_LuaInteractive("x+2",false,a));assert(output.find("42")!=std::string::npos);
 fail_allocation=true;
 assert(!Debugger_LuaInteractive("local never_compiled_allocation_test = {}; return never_compiled_allocation_test",false,a));
 fail_allocation=false;
 assert(!interactive_active && lua_gettop(interactive_lua)==0);
 assert(Debugger_LuaInteractive("assert(x==40)",false,a));
 output.clear();
 assert(Debugger_LuaInteractive("print('print'); dbg.log('direct'); dbg.logf('%s:%d','formatted',42); assert(coroutine.resume(coroutine.create(function() dbg.log('coroutine') end)))",false,a));
 assert(output.find("print")!=std::string::npos && output.find("direct")!=std::string::npos && output.find("formatted:42")!=std::string::npos && output.find("coroutine")!=std::string::npos);
 assert(log_output.empty());
 assert(Debugger_LuaInteractive("assert(require('worker_native').answer()==42)",false,a));
 assert(Debugger_LuaInteractive("fixture.lua",true,a));
 assert(Debugger_LuaInteractive("assert(file_result==42)",false,a));
 assert(!Debugger_LuaInteractive("missing.lua",true,a));
 assert(!interactive_active && lua_gettop(interactive_lua)==0);
 assert(!Debugger_LuaInteractive("error('expected failure')",false,a));
 assert(!Debugger_LuaInteractive("error(42)",false,a));
 assert(output.find("non-string error")!=std::string::npos);
 assert(!interactive_active && lua_gettop(interactive_lua)==0);
 assert(!Debugger_LuaInteractive("return setmetatable({}, {__tostring=function() error('format failure') end})",false,a));
 assert(!interactive_active && lua_gettop(interactive_lua)==0);
 assert(Debugger_LuaInteractive("assert(x==40); assert(type(io.open)=='function' and type(package.loadlib)=='function'); assert(not pcall(dbg.step)); setmetatable({}, {__gc=function() dbg.log('finalized') end})",false,a));
 admitted=false;assert(!Debugger_LuaInteractive("x=9",false,a));admitted=true;
 assert(Debugger_LuaInteractiveReset());assert(!interactive_lua);
 assert(output.find("finalized")!=std::string::npos);
 assert(Debugger_LuaInteractive("assert(x==nil)",false,a));assert(Debugger_LuaInteractiveReset());
 lua_State *other=luaL_newstate();luaL_openlibs(other);lua_pushstring(other,"outside");Debugger_LuaLog(other);lua_close(other);
 assert(log_output.find("outside")!=std::string::npos);
 lua_State *file_state=lua_newstate(allocator,nullptr);assert(file_state);
 lua_pushcfunction(file_state,LocalEvaluateFile);
 lua_pushlightuserdata(file_state,const_cast<char *>("fixture.lua"));
 assert(lua_pcall(file_state,1,0,0)==LUA_OK);
 lua_getglobal(file_state,"file_result");assert(lua_tointeger(file_state,-1)==42);lua_pop(file_state,1);
 lua_pushcfunction(file_state,LocalEvaluateFile);
 lua_pushlightuserdata(file_state,const_cast<char *>("missing.lua"));
 assert(lua_pcall(file_state,1,0,0)!=LUA_OK);lua_settop(file_state,0);
 fail_allocation=true;
 lua_pushcfunction(file_state,LocalEvaluateFile);
 lua_pushlightuserdata(file_state,const_cast<char *>("fixture.lua"));
 assert(lua_pcall(file_state,1,0,0)!=LUA_OK);
 fail_allocation=false;lua_close(file_state);
 puts("actual Interactive evaluator/sink Lua fixture PASS (native admission mocked; not production integration)");
}
'''
(w/'test.cpp').write_text(preamble+logs+register+section('void LiveFields(lua_State *L,int table,const char *const *allowed) {', 'lua_Integer LiveInteger')+section('void LiveCheckpointArguments(lua_State *L) {', 'int LiveCheckpoint(lua_State *L) {')+initialize_fixture+section('int LiveInitializeLean(lua_State *L)', '// Protect argument/metadata/closing projection')+section('int LivePrepareDispatch(lua_State *L)', 'void LiveSliceBody()')+section('static int InteractiveOutput(lua_State *L)', 'bool Debugger_LuaRunScriptInStoppedState')+main)
flags = shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','lua5.4'],text=True))
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',str(w/'test.cpp'),*flags,'-o',str(w/'test')],check=True)
subprocess.run(['c++','-std=c++14','-shared','-fPIC',str(root/'tests/machine-service/worker-native.cpp'),*flags,'-o',str(w/'worker_native.so')],check=True)
(w/'lean-dispatcher.lua').write_text("return {live_dispatch=function() end, live_finish=function() end}")
(w/'dispatcher.lua').write_text("assert(type(dbg.live.task)=='function'); return {}")
(w/'bad-dispatcher.lua').write_text("return 42")
(w/'fixture.lua').write_text("file_result=require('worker_native').answer(); print('file result', file_result)")
env=dict(os.environ,LUA_CPATH=str(w/'?.so'))
subprocess.run([str(w/'test')],cwd=w,env=env,check=True)
