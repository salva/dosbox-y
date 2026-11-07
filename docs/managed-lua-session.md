# Managed Lua host sessions

`-lua-driver-session` is valueless and exclusive with `-lua-driver-file` and
`-lua-driver-code`. Program selection, native frontends and existing FILE/INLINE
semantics are unchanged. This entry uses the existing initialized Lua worker,
native owner and bindings; it is trusted code, not a sandbox.

## Combined input and installation

Foundation: `7883a116990aef64402ca4f667e9506ba3691422`, engine29/representation9.
Public dependency: https://github.com/salva/lua-repl-mcp at
`0b8d271911c17ce762246afa86b74ab20e999343`, tree
`4a8e9b8d4976152460398740e8de75ad2fff19bb`. Its package name/version
`lua-repl-mcp 0.1.0-1` alone does not identify these bytes. Archive SHA256:
`0ff6919894b6716f9ff46ef5ef39832147686379135d43b2dc4c7ed51b59363f`.
No dependency license or shipping decision is changed here.

Build DOSBox with `--enable-debug=heavy --enable-debug-lua --enable-sdl2`
(and optionally `--enable-dosbox_agent`). Match the embedding Lua ABI for C
extensions. Tested here with system Lua5.4, GCC13.3, SDL2, FreeType; OpenGL and
avcodec disabled. Install the pinned module using its rockspec in an isolated
Lua5.4 tree, including `luaposix 36.3-1` for the **manager/default worker**.
Alternatively install the public `lua_repl_mcp/` files and launcher unchanged
into that tree and build luaposix with its supplied `build-aux/luke`.
The DOSBox host's pure module imports do not require posix. Set installation-only
`LUA_PATH`/`LUA_CPATH` to that tree (unset versioned overrides when testing).
Libraries and ordinary C `require` remain available in the same worker state.

Launch installed `lua-repl-mcp`; supply this literal custom argv to
`session_create.command` (absolute paths; no shell evaluation):

```json
["/absolute/dosbox-x", "-conf", "/absolute/fixture.conf", "-restore-state", "/absolute/image.sav", "-lua-driver-session"]
```

The manager allocates a fresh disposable process, supplies inherited blocking
stream socket FD3 and `LUA_REPL_MCP_SESSION_FD=3`,
`LUA_REPL_MCP_ALLOCATION_GENERATION`, `LUA_REPL_MCP_LIMITS` (all seven budgets).
Do not start this mode in a shell without that ABI. Limits are decoded by
`require('lua_repl_mcp.bootstrap').decode(fd,generation,limits_json)`, then the
host calls `require('lua_repl_mcp.session').serve(adapter,options)` in the same
state. Adapter functions are plain borrowed read/write/flush, not methods.
Ready occurs only after actual native initialization and optional fresh restore
has installed a stopped image. With no restore, `-lua-driver-program C:/TINY.COM`
can select natural program entry. Never combine program replay with restore.
Used-target live restore remains refused. Restored root termination disposes the
process; only explicit step/Run advances the guest. No producer suffix replay.

## Ownership and stopping

Startup adopts FD3 and promptly marks it CLOEXEC, rejects non-stream/nonblocking
endpoints; worker only borrows it. recv/send retry EINTR, retain partial progress,
and send uses MSG_NOSIGNAL. No proxy, second evaluator, engine or polling thread.
The native owner wakes external shutdown with shutdown(SHUT_RDWR), joins the
worker after claimed effects and observer settlement, then closes the exact
owned descriptor. Startup failure closes without spawning a worker. Native
terminal lifetime revokes effect admission but permits the current evaluator's
result write/flush before session close, with no next chunk. Native lifetime is
not external interruption. Native Stop is not destructive manager session_kill.
External interruption can lose a result after effects; do not replay it.
Hard kill does not promise Lua finalizers, rollback, restored host files, or
archive cleanup. Cooperative hooks cannot preempt foreign C or blocking fsync.
A transport stall can block terminal flushing until external manager disposal.
A private protocol close ends the worker, not necessarily the emulator process;
the owning manager must confirm process exit/disposal. Leases, claimed effects,
ExitFence and native-tool adapter remain their existing owners' responsibility.
Opaque dosbox contexts remain local; return explicit JSON projections.

## Reproducible checks

Set `SAIVAGE_CARD_WORK_ROOT` to a task-owned directory,
`DOSBOX_TEST_BINARY` to the built executable, `DOSBOX_SESSION_MODULE` to the
installed Lua module root (the directory containing `lua_repl_mcp`), and
`DOSBOX_SESSION_INSTALL` to the install prefix. Xvfb and a C compiler are needed.

* `python3 tests/machine-service/managed-session.py`: actual shared worker/native
  engine, installed module, C require/full libs, persistent globals, errors,
  once-only effects, fresh restore, repeated stopped saves, explicit continuation,
  root exit result before EOF.
* `python3 tests/machine-service/managed-session-mcp.py`: actual installed stdio
  MCP default/custom create/execute/wait/kill, native access, subprocess CLOEXEC,
  confirmed disposal. Not actual Saivage/provider or independent acceptance.
* Compile `session-worker.cpp` plus `src/debug/lua_driver_worker.cpp` with
  `-std=c++17 -pthread -Iinclude $(pkg-config --cflags --libs lua5.4)` and run
  with installed `LUA_PATH`: borrowed FD through join, shutdown while idle,
  claimed native request, observer settlement, peer transport close. This is
  a transport/ownership fixture, not four actual emulator shutdown trials.
* Existing `check-worker.sh`, `options.cpp`, `lua-production-runtime.py` preserve
  one-shot FILE/CODE/native behavior. Parent still owns full isolated actual
  Saivage qualification and independent affected acceptance.
