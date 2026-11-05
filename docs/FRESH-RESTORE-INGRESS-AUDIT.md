# Fresh restore ingress audit — 2026-11-05

## Superseding finite reconciliation — 2026-11-05

Independent [[card:card-n-c-k-b]] status6 approved bounded f1d0 evidence and
unchanged production6ae12, **not** this document's former completeness claim.
The following inventory corrects the omitted consumers and dispositions; it is
not full §7 or hardware/constructor/close/timing acceptance.

- `src/debug/lean_live.inc` LeanState / `state_file(action=load)` retains its
  argument/context/busy/entry eligibility checks (which can return earlier).
  Eligible requests call Debugger_LocalDriverLoad -> DEBUG_NativeStateFile ->
  loadFile -> unconditional fresh-launch refusal. Terminal is completed=false,
  loaded=false, retired=true, code=the explicit fresh-launch-required error,
  usability comes from
  current admission. No target install or context retirement occurs. Entry
  checks are not a supported restore capability; public state_file save and
  other lean operations remain. The helper now explicitly describes refusal.
- `debug.cpp` legacy local startup archive branch (after natural program entry,
  before worker) calls that same refusing helper and E_Exit on error. No hidden
  decoder exists. The CLI now rejects `-lua-driver-state` during option
  validation before natural execution/worker, and help calls it unsupported.
  The defensive internal branch remains refusing, not an active import seam.
- For actual startup import use `-restore-state /path/archive.sav` with matching
  initialized hardware/mounts and **without** `-lua-driver-program` or
  `-lua-driver-state`; optionally add `-lua-driver-file script.lua` or
  `-lua-driver-code '...'`. Debugger is the debug-build default; no-debug uses
  `-restore-console`. These are the existing common fresh startup seam, not a
  fictitious replacement command or used-target alias.
- Selected-slot refusal now reports its concrete configured savedir/slot path
  (or selected explicit savefilename); explicit loadFile reports its supplied
  archive. It names `-restore-state <archive>` and never creates directories,
  opens archives, acquires leases or advances guest to report refusal. Invalid
  native slots refuse explicitly. Saving/selection/info/deletion unchanged.
- Legacy force menu now says unsupported/fresh-launch-required and is disabled.
  Its retained callback only reports refusal; no flag toggling or force bypass.
  Config preference remains inert for compatibility, not a restore capability.

New regression coverage extends used-target-ingress.py with resolved default,
custom-dir and explicit-file diagnostics, and real stopped-save.py slot UI with
exact savedir path/action, repeated stopped saves and unchanged held owners.
Runtime identities/results belong to current owner status and reconciliation
handoff; reduced-host checks do not replace actual UI evidence.

## Prior bounded inspection provenance

The original f1d0 documentation/test-only successor inspected
production6ae12f2122997197cfbdd1ba789e48eb82009b07. Its inventory omitted the two
legacy consumers now included above; it was not a final §5 reconciliation.
The current candidate includes the explicit production corrections above and
requires its own builds/runtime evidence. Governing design lives at
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
common output traversal; private MemoryInput remains unused transport code,
not an active alternate decoder/import seam. Failed target must
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
