# Fresh restore ingress audit — 2026-11-05

Author inspection of production6ae12f2122997197cfbdd1ba789e48eb82009b07,
plus this documentation/test-only successor. This is final direct-caller
inventory for the currently implemented ingresses, not a universal runtime or
§7 requirement-record approval. Governing design lives at
`/work/dosbox-y/docs/PLANS/FRESH-TARGET-RESTORE-DESIGN.md`, not this checkout's
absent PLANS copy. Accepted brief supersedes its historical pending labels.

## Consuming boundary

`src/misc/savestates.cpp` `SaveState::install(Input&)` has the only source
component `setBytes` dispatch in src. Its only call is `restoreFreshStartup`.
`include/dosbox.h` keeps install and transport internals private. CLI sdlmain
calls fresh startup at the top-level unused startup before debugger/FILE/INLINE
attachment or normal RunMachine. It constructs the native owners, opens source
read-only, installs common streams, settles exact archive and validates/selects
root. No used-target transport import was found. Memory checkpoint capture uses
common output traversal; no second memory decoder remains. Failed target must
be disposed; no unchanged-destination/rollback/used-target preservation claim.

## Caller inventory

- Native hotkey/slot: savestates `LoadGameState` -> `SaveState::load` ->
  `loadFrom`, unconditional false/log fresh-launch-required. Press-release does
  nothing. `LoadGameState_Run` is same wrapper, no separate decoder. Mapper
  registration references this handler; menu mapping shares it.
- Native explicit file: `loadFile` -> same refusal before source I/O.
- Debugger slot: `DEBUG_NativeStateSlot(load=true)` refuses through loadFrom
  **before** attachment/acquisition. Save branch retains exact lease.
- Debugger STATEREAD/path: command parser -> DEBUG_NativeStateFile(load=true)
  -> loadFile before admission/acquisition. Effects initialized empty, not a
  falsely successful restore. Lua/archive frontends project this shared path.
- Process-local checkpoint: `checkpointLoad` unconditionally false without
  inspecting bytes. Lua `LiveCheckpoint` denies op=load with explicit fresh
  disposable-launch diagnostic before capture/control ownership; saves remain.
- Direct search across src found no component setBytes bypass, checkpointLoad
  caller or alternate native load engine. Direct consumers of these public
  APIs therefore cannot force used-target import. No force option added.
- `SaveGameState` checks save_state_frontier before focus/key/UI operations;
  direct `SaveState::save` also guards. Timed VGA autosave goes through this
  native entry. Unsafe dispatch refuses rather than advances to manufacture
  a capture point. This is not full integrated timed VGA qualification.

## Reproducible checks and bounds

Run `python3 tests/original-foundation/used-target-ingress.py` and
`python3 tests/original-foundation/autosave-frontier.py` with
SAIVAGE_CARD_WORK_ROOT set. proc-6a4b55c06751 exit0 executes original extracted
load/loadFrom/loadFile/checkpointLoad method bodies with invalid slots,
nonexistent/untrusted file names and malformed memory; all refuse debug/native.
The latter executes actual autosave/slot bodies and timed VGA producer with
reduced UI doubles, denied before focus/key/UI/save effects, UBSan both profiles.
These are reduced-host tests, not integrated runtime frontend parity. Existing
final6ae12 actual Lua E2E and native SB evidence retain exact applicability in
FRESH-RESTORE-NATIVE-CONSOLE.md; no production rebuild needed for header comment.

Inspection commands: recursive native grep of `SaveState::instance`,
`checkpointLoad`, `.setBytes(`, `LoadGameState`; inspect savestates install/load,
include/dosbox.h access, debug.cpp native state branches and debug_lua.cpp
LiveCheckpoint. Source traversal via dedicated grep returned empty due project
search exclusions; explicit native command scan used instead, not absence proof
from filtered search. Early missing include/savestates.h, checkout PLANS design
and optional ingress glob were inspection failures, not missing requirements or
passing tests.

## §7 reconciliation boundary

This audit reconciles current capture/load API comments and actual ingress
support: repeated stopped save/selection/info/deletion remain; restore is fresh
startup only, native/debugger/slot/checkpoint used-target loads unsupported.
No promise of full executable equality/global Prepared/unchanged used-target
recovery is reinstated. Effective hardware and native extents are still bounded;
full ROM/DOS/HMA/UMB/private/MSCDEX lazy extent audit, transitive constructor and
close races and wider API/CLI/Lua/operator/test documentation reconciliation
remain. Parent/authorized requirement owner retains accepted-record updates and
composed acceptance; this author document cannot rewrite those records. No
unsupported feature reduction or new all-hardware gate is inferred.
