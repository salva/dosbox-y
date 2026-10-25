// GPL-2.0-or-later. Actual C ABI module loaded by the worker fixture.
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
static int answer(lua_State *L) { lua_pushinteger(L,42);return 1; }
extern "C" int luaopen_worker_native(lua_State *L) {
    lua_newtable(L);lua_pushcfunction(L,answer);lua_setfield(L,-2,"answer");return 1;
}
