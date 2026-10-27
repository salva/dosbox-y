#!/usr/bin/env python3
"""Exercise actual state-file request validation with real Lua (no native effects)."""
import os
from pathlib import Path
import shlex
import subprocess
root = Path(__file__).resolve().parents[2]
area = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'lua-state-validation'
area.mkdir(exist_ok=True)
source = (root/'src/debug/debug_lua.cpp').read_text()
start = source.index('int LiveStateFile(lua_State *L) {')
end = source.index('#include "lean_live.inc"', start)
preamble = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
static void PredicateGuard(lua_State *){}
static bool Debugger_PredicateHeld(){return false;}
'''
main = r'''
static void check(lua_State *L,const char *op,size_t size,const char *expected) {
 lua_pushcfunction(L,LiveStateFile);lua_newtable(L);
 lua_pushlstring(L,op,size);lua_setfield(L,-2,"op");
 lua_newtable(L);
 lua_pushliteral(L,"00000000000000000000000000000000");lua_setfield(L,-2,"instance_id");
 lua_pushliteral(L,"1");lua_setfield(L,-2,"owner_token");
 lua_pushliteral(L,"1");lua_setfield(L,-2,"state_epoch");
 lua_setfield(L,-2,"expected");
 assert(lua_pcall(L,1,1,0)==LUA_ERRRUN);
 assert(strstr(lua_tostring(L,-1),expected));lua_settop(L,0);
}
static int accessed(lua_State *L) {return luaL_error(L,"METAMETHOD EXECUTED");}
static void metatable_check(lua_State *L,bool nested) {
 lua_pushcfunction(L,LiveStateFile);lua_newtable(L);
 lua_pushliteral(L,"save");lua_setfield(L,-2,"op");
 if(nested)lua_newtable(L);
 lua_newtable(L);lua_pushcfunction(L,accessed);lua_setfield(L,-2,"__index");
 lua_setmetatable(L,-2);
 if(nested)lua_setfield(L,-2,"expected");
 assert(lua_pcall(L,1,1,0)==LUA_ERRRUN);
 assert(strstr(lua_tostring(L,-1),nested?"plain identity":"plain object"));
 lua_settop(L,0);
}
int main() {
 lua_State *L=luaL_newstate();assert(L);
 metatable_check(L,false);metatable_check(L,true);
 const char *unsupported="native persistent manifest/lease admission not installed";
 for(auto op:{"save","load","inspect","readiness"})check(L,op,strlen(op),unsupported);
 const char invalid[]="save\0junk";
 check(L,invalid,sizeof(invalid)-1,"invalid state_file operation");
 check(L,"saveX",5,"invalid state_file operation");
 check(L,"",0,"invalid state_file operation");
 lua_close(L);puts("actual state-file validator PASS (no native effect)");
}
'''
validator_start = source.index('void LiveFields(lua_State *L,int table,const char *const *allowed) {')
validator_end = source.index('const char *LiveString(', validator_start)
preamble = '#include <initializer_list>\n' + preamble
(area/'test.cpp').write_text(preamble+source[validator_start:validator_end]+source[start:end]+main)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','lua5.4'],text=True))
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',str(area/'test.cpp'),*flags,'-o',str(area/'test')],check=True)
subprocess.run([str(area/'test')],check=True)
