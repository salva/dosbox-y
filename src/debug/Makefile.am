AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libdebug.a
libdebug_a_SOURCES = debug.cpp debug_gui.cpp debug_disasm.cpp debug_common.cpp debug_common.h debug_inc.h disasm_tables.h debug_win32.cpp debug_mcp.cpp debug_mcp.h

if C_DEBUG_LUA
AM_CPPFLAGS += $(DEBUG_LUA_CFLAGS)
libdebug_a_SOURCES += debug_lua.cpp debug_lua.h lean_live.inc lua_driver_worker.cpp
endif
