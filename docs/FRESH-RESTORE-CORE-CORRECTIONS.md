# Fresh disposable restore: core correction and qualification — 2026-11-06

Author delivery against review R1–R3 (`record:///review.md?card=card-n-c-k&v=1`),
not independent approval. Source is FINAL2033 ancestry plus the correction
committed with this document. Evidence root below is
`/work/dosbox-y/.saivage/work/cards/card-n-c-k-a/` (W). Logs are retained work
evidence, not versioned fixtures. No prior binary is relabeled current.

## R1 — effective initialized CPU owner

`src/cpu/cpu.cpp` now exports the constructor's resolved policy, architecture/
prefetch width, PSE presence/width, instruction capabilities, REP limit and
parsed processor serial. `src/misc/savestates.cpp` replaces **CPU-owned**
requested properties with these effective values, alongside existing initialized
FPU, CPUID vendor, resolved RAM and loaded immutable inputs. The CPU section
also contains BIOS and frontend properties: these are deliberately not dropped.
No second parser/validator or executable/backend equality is introduced.

The resolved policy is frozen at `CPU::Change_Config`, not sampled from the
guest-mutated decoder, HLT wrapper or current cycle budget during capture.
It includes decoder type/auto policy, cycle mode/limit/adjustment, RDTSC rate,
segment/ISA/fault controls and resolved dynamic cache/paging policy. Auto paging
is a policy, not its transient startup decision. Architecture includes initialized
prefetch width; queue width is cleared before selecting a new CPU type.
PSN is disabled outside its owning architecture; enabled PSN compares parsed
hi/lo values, not separators. MSR-off disables syscall; ENTER/LMSW auto resolves
false; REP negative resolves four; default mixed CPU auto PSE is PSE36.

Actual current integrated positives and mismatch controls, proc-23a8a259b175:

| Test / retained directory | Positive equivalence | Genuine refusals |
|---|---|---|
| `fresh-effective-machine.py`, `effective-machine-fs_6o7kf` | inactive PSN spellings; Pentium III PSN hyphens/spaces; paging auto/-1 | architecture/prefetch, enabled PSN/disabled PSN, cycle budget, forced paging |
| `fresh-effective-instructions.py`, `effective-instructions-smfrwtdo` | MSR-off syscall on/off, ENTER/LMSW auto/false, REP -1/4 | MSR, isolated syscall with MSR on, CMPXCHG8B, ENTER, LMSW, REP8 |
| `fresh-effective-pse.py`, `effective-pse-jjcmqfcs` | auto/true/pse36 | none/false/pse/pse40 |

Every positive uses new capture → fresh held save/delay → explicit step/Run.
Every mismatch exits1 at Config_Identity before evaluator, with disposable-target
failure and unchanged archive. `config-identity.py` is actual hash traversal
with section/hash/CPU doubles, **extraction**, not integrated completeness.
Representation/engine9 and payload codec remain unchanged; previous digests can
conservatively reject, no archive migration promise.

Early effective-machine attempts `effective-machine-aobnvf00` and `-p0ongtig`
failed because invalid numeric requested settings were wrongly assumed equivalent:
property range handling changes their effective values. Removed those false
positive expectations, not the production guards. Earlier PSE auto→none test
was also a wrong mixed-CPU expectation. Historical failures remain evidence.

## R2 — native owner and immutable-root acceptance map

The governing recipe is `VM_ConstructFreshNative` in `src/gui/sdlmain.cpp`:
one-shot unused powered/quiescent target; reject nonempty devices; original BIOS
native construction → original DOS kernel native construction → shell messages
and native shell construction. No POST/EXEC/AUTOEXEC/shell guest replay. Common
`restoreFreshStartup` constructs before opening/consuming the archive and before
debugger/Lua attachment. Nested guest dispatch is refused. Any construction,
member, CRC or close failure disposes rather than admitting an evaluator.

| Native owner / consumption boundary | Binding and installed-span argument | Actual targeted coverage |
|---|---|---|
| BIOS `BIOS::ConstructFreshNative`, native allocator codec | original allocation order, bounds, role/location/span, ROM/fixed/free/string extents | BIOS binding negative receipt below; current matrix normal BIOS/service startup |
| Shell `SerializeShellBindings` | original roles, host handlers, slots/type/location/span, native FE38/return bodies; IVT remains guest-owned | current strict Shell slot negative in native console; shell-child matrix; controlled-body negatives below |
| DOS `DOS_TableBindings`, DOS callback codec | original allocation records and exact next-free cursor, table bodies; nine callback roles/spans, IHSEG one-byte IRET or clear-TF helper; HMA CPM body | current IHSEG/clear-TF, files/search/UMB/HMA cases; settled private-cursor negatives and native body negatives |
| XMS binding/body consumer | enabled/count/location/slot/type/span10; hookable first5 bytes are guest hook, native suffix is checked | XMS/HMA/EMS/A20 plus legitimate XMS hook; slot/body negative receipt |
| EMS `EMS::NativeBindings` | VDMA/INT67/VCPI/WinVM86/v86 monitor roles, slots/types/locations/spans; private physical VCPI suffix checked without toggling saved A20 | current EMS/HMA/A20 cases and binding/body negatives |
| INT10/VESA owner | fresh callback handler/slot/IRET and ROM/reference metadata; post-RAM controlled bodies, no IVT-default equality | current INT10/VESA/display cases; malformed native body negatives |
| Mouse/keyboard/input | reconstructed service callbacks and owner field/latch bounds, not raw host objects | current service/active input cases; malformed Mouse/Keyboard and late-member failures |

Current `fresh-root-matrix.py` proc-e6226db9d9de,
`fresh-root-matrix/run-g_4rup4u` (full log `build/owner-root-matrix.log`): all33
cases pass, including SB FIFO, PIT/speaker, files/shared handles/search with
added/deleted entries, UMB/XMS/HMA/EMS/A20, hooks, display and roots below.
No waveform/rendered-frame/physical-input or universal lazy-native-history claim.

Native negative receipt proc-58acfdf31fa3 is retained on its earlier correction
input: BIOS/Shell/XMS/EMS descriptor/body, INT10/VBE, PSP signature/MCB ownership/
parent/stack/vector failures. These consumer implementations are unchanged by
this CPU metadata correction; that is reuse, **not a current-input rerun**.
Settled DOS cursor correction3de342, retained in2033, compares actual fresh
next-free cursor, not just in-arena range (see `FRESH-RESTORE-PRIVATE-CURSOR.md`).
Earlier broad failure matrix had a Python late-binding fixture bug: it flipped
RAM0 instead of Shell body, legitimately accepted. Corrected fixture/subset,
not deleted guard. Earlier1629 cases/skipped tail are not a full passing matrix.

### Termination: transitive effect boundary

Capture selects current SDA PSP, validates PSP/MCB ownership/size/signature and
saved parent/vector/stack constraints, excludes shell root, then stores one-shot
nonzero invocation-owned immutable root. Archive hooks need not equal defaults.
`DOS_Terminate` tests selected root **before** return-code writes, self-parent
return, UTF8 provider shutdown, parent/vector/file/free/stack/cycle suffix.
`RestoredSession::terminate` copies exact terminal facts and throws `RootExit`.
A newly EXECed descendant is not the selected root and uses ordinary DOS return;
AH31/INT27 service-prefix effects are allowed, not interrupt-wide transactions.

The typed exception crosses callback/interpreter frames without a normal callback
return. `debug_common.cpp` explicitly rethrows before decoder-error catch-all;
shell catch sites explicitly rethrow; CPU exception cleanup resets its nesting
level and rethrows. Outer `sdlmain.cpp` catches outside guest frames, publishes
facts, stops run_machine and suppresses ordinary DOS shutdown continuation.
Restored reboot/BOOT is failure/disposal, not a new guest bootstrap. The stale
return fence is secondary, not the root detector. Ordinary nonrestore execution
never selects a root and preserves the original suffix.

Actual same-input consumers:
- Current33-case matrix: custom INT22, INT20, AH00, INT27, AH31, self-parent,
  child, child-TSR and shell-child. Exact terminal facts, not just process exit.
- `fresh-native-roots.py`, proc-e6226db9d9de, `native-roots-tpo8qdil`:
  **same nine archives** through no-debug/no-Agent/no-Lua `-restore-console`,
  held/status/delay invariant then explicit Run and exact disposal.
- `fresh-startup-terminal.py`, FILE and INLINE: custom INT22
  `terminal-5vn1n5s2` (proc-9cdc29f8c445); INT27 `terminal-rzpcb6ra`, AH31
  `terminal-izrd0a9a`, self `terminal-20_8vxhi`, child-TSR `terminal-uvaoyqdj`
  (proc-dcc606ebc31d). Actual C-blocked __gc retained process until invocation's
  release marker; terminal response/revoked context delivered first. Four latter
  cases also observe/save/delay before Run. First INT27 attempt
  `terminal-eocqr3ke` supplied false resident expectation: correct native
  resident=true caused assertion failure, then corrected harness settings pass.

Unsupported external boot/nonempty devices/shell root/unrepresented lazy native
histories remain explicit, not a substitute for required represented cases.

## R3 — faults, time, exact operation, publication and close

| Requirement | Evidence and limits |
|---|---|
| CPU/memory/PIC/input/late failure | proc-0fe3bab72f7b `fresh-native-entry/failure-mr5hncot`: valid-CRC CPU/Memory truncation, CPU pmode/stack/code/trap booleans, Memory geometry/LFB/handle cycle/range/handler tags plus Mouse/Pic/late Keyboard/CRC/missing owner. Each exit1 before evaluator/continuation; source and attempts unchanged. Earlier owners may have installed: disposal, not rollback. |
| Source close failure | Independent k-b status9 on exact2033 confirms installed=true source-fclose EIO rejection; unchanged close implementation reused, not ceremonially rerun or relabeled current. |
| Startup/attachment/delay held, meaningful continuation | current33-case matrix; `native-console-si0e_hma` proc-e6226db9d9de; FILE/INLINE `lua-0g1fedr5` proc-0fe3bab72f7b: repeated held saves/observation/delay, six-owner invariance, CPU prefix/RAM equality, full libraries/C require/dofile/error and close; explicit step/Run only advances. |
| PIT/pacing algorithms | proc-36aa4cc871e8 `timer-consumption.py` and `dosbox-pacing.py`: actual owning codec/time-consumer extraction, saved time/bounds, hold no clock sample, first explicit continuation rebases host pacing once. Integrated PIT/speaker/SB and held invariance above, not a universal timing benchmark. |
| Deadline through finalization | proc-3656f6b6d9ad `output-deadline-ds5s_uq3`, `fresh-output-deadline.py` + `output-deadline.c`: FILE/INLINE actual fsync delayed300ms,100ms save deadline, marker proves finalization entered; timeout/retired/not completed/not published/usable, old destination/source retained, temp removed, close marker, unchanged registers. No hard fsync/C preemption. |
| Atomic output/effects | proc-36aa4cc871e8 `state-archive-output.py`: actual retained ZIP output ASan/UBSan failures/publication/repeated publication/cleanup. Intentional injected errno39 cleanup failure diagnostic is expected, not erased. |
| Exact claimed operation on close | `tests/machine-service/handoff.cpp`, proc-f24ce36d30f4 UBSan: claimed Save path, published-but-not-completed/durable terminal effects preserved across close, no next grant, retirement held until worker-close fence. Also read/event/verdict close races. This is real transport code with native result fixture, not an integrated filesystem race. |
| Lua/C close fence | integrated terminal blocked-C consumers above; `runWorker` closes handoff, disables bytecode hook for finalizers, calls lua_close, then publishes luaClosed. `DEBUG_CloseLocalLuaDriver` revokes matching grant, copies root terminal, drains read/verdict/response, waits fence before LocalDriverRetire/join. Production save is synchronous on native thread: normal teardown cannot free its stack-owned stream concurrently; transport retains claimed request until exact completion. Arbitrary blocked native I/O/C can prevent progress; no bounded graceful-close success claimed. |
| Worker full libraries and errors | proc-dcc606ebc31d `check-worker.sh` actual Lua+C transport fixture; integrated FILE/INLINE above. Fixture native settlement is not emulator integration. |
| Autosave | proc-155561d1ddcc `stopped-save-vertical/run-0nbzbzpx` actual timed VGA autosave after authorized Run→BREAK: unsafe callback refused≥2 times, no callback archive, later held save succeeds. CPU metadata does not change admission. proc-36aa4cc871e8 `autosave-frontier.py` actual slot entry with reduced UI doubles, debug and no-debug denied frontier no focus/key/UI/save effects. No no-debug timed-runtime autosave success claimed. |
| Ingress/other APIs | settled1003ee `FRESH-RESTORE-INGRESS-AUDIT.md`; current proc-36aa4cc871e8 actual load/loadFrom/loadFile/checkpointLoad extraction refuses before I/O/consumption. Existing Run/step/stop/frontend APIs unchanged. |

proc-3656f6b6d9ad then requested nonexistent `fresh-lua-entry.py`: failed/skipped
tail, not acceptance. Correct `fresh-startup-lua.py` and extraction/diff checks
passed proc-0fe3bab72f7b. Earlier MEMORY_FAULTS variable did not enable Memory
geometry tests; corrected MEMORY_SAFETY invocation above actually executed them.

### §7 dated r2 successor / interface disposition

This document is the dated **core** successor to approved r2 and historical Astra,
not deletion or retrospective acceptance of those originals. Governing design
`docs/PLANS/FRESH-TARGET-RESTORE-DESIGN.md`62057 bytes,
SHA2567204d48e1ab8212c7fb7cb19072786f0bd7022eda0db18122e0f50ab49be23ba,
supersedes mandatory global prepare/transaction and used-target preservation.
Original capture/install traversal and one owner codec remain ZIP/memory; no
rollback/replay, serialized Lua stack or raw host-resource identity.

- CLI: preconfigure matching hardware/mounts, use `-restore-state archive`.
  Restore precedes immediate debugger/`-lua-driver-file FILE`/`-lua-driver-code INLINE`.
  No-debug uses `-restore-console`, status is held, only run continues; quit/EOF
  disposes. `-silent`/`-test` are not backend-equivalence evidence.
- Debugger/Bridge/native/RPC-facing load/STATEREAD/checkpoint and slot/menu/hotkey
  load into a used target return fresh-launch-required, resolved path/action;
  force does not bypass. `-lua-driver-state` is explicitly unsupported, not a
  hidden live reload. Existing unrelated observation/execution/stop APIs remain.
- Repeated STATEWRITE/save, slot/file selection, metadata/info and explicit
  deletion remain. Autosave refuses unsafe callback frontier without advancing
  guest just to save. Memory capture is bounded transport, no second decoder.
- Lua API: context/lease is exclusive; save returns exact retirement/publication/
  cleanup/usable facts, not archive-existence success. Full libraries, C require,
  errors/finalizers/close retained. Terminal root response revokes context before
  Lua close. Cooperative deadline is not C/OS/finalizer preemption. If blocked,
  retain ownership/incomplete truth, never silently resume/free beneath work.
- R3 files: non-destructive native reopen/shared handles/flags/cursors/timestamps
  and current-directory searches remain; changed entries alone are not failure.
  No host-content rollback, guest INT21 replay or changed-file restoration.
- Tests: use fresh success/failure-disposal and legitimate-hook/native consumers
  above. Unchanged-used-target/global-Prepared/source-shape/live-load/replay/
  exact-build-equality assertions are obsolete, not a reason to remove safety.

Broader fonts/providers/peripherals/frontends/composed acceptance remains parent
allocation. MIDI unqualified follow-up, not a universal gate. Historical positive
SDL→dummy audio-preserving surface backend witness remains bounded; no universal
hardware certification. Distinct fresh native allocations are constructor-owned,
not serialized pointer rebinding. No admission of arbitrary external-boot/device
or lazy-allocation histories.

## Exact build and reproduction

proc-23a8a259b175 sequential Lua/native builds, `build/owner-final-{lua,native}.log`:
- Lua binary `8c03db06ae096401d96a0258b5cad6ee22f32bafa028992b690016307f8794de`;
  config.h `f3b2dd74f8cb9d7526b5bd8f976af4e04d416227c1dd5bc009dfcc7663e39975`;
  config.status `55409ae4f12511c7f540aa473709449db0c8381c58620fca65360bf97f94715a`.
- native binary `65201399a89ee65c40136074640409a0aa024ad22c09fd960f676513de54eddf`;
  config.h `5a6293639a5798fdb3a5160d194f3529ca42ca09e7b0be24d0c11c0b298a132b`;
  config.status `b619e6a0b42d8eb78323ec9a2e7415a6ac35aa54841ecada234bfa8b039514e8`.
Paths W/build/{lua,no-debug}/src/dosbox-x. Native undefines DEBUG/DEBUG_LUA/
DOSBOX_AGENT/HEAVY_DEBUG. Hashes are evidence, not compatibility requirements.

Set DOSBOX_TEST_BINARY, DOSBOX_TEST_NATIVE_BINARY, SAIVAGE_CARD_WORK_ROOT to
those inputs. Run effective-{machine,instructions,pse}, root-matrix then
native-roots (DOSBOX_TEST_ROOT_MATRIX from first output line), native-console.
Its producer.log first line identifies current SB capture; pass its first.sav
as DOSBOX_TEST_ARCHIVE, with AUDIO=1 LUA=1 CORE_TRUNCATION=1 CPU_BOOLS=1
MEMORY_SAFETY=1 (each prefixed DOSBOX_TEST_) to fresh-startup-failure.py.
Use same archive for fresh-output-deadline.py and fresh-startup-lua.py.
Terminal test uses root-matrix producer archives and explicit EXIT_CODE,
RESIDENT/SELF_PARENT flags, BLOCKED_FINALIZER=1/OBSERVE_BEFORE_RUN=1 as applicable.

No service/publication/source switch/history rewrite/sibling mutation/unknown
cleanup. Historical proc-ca8d590fa289, Xvfb584020/display106 and incomplete
archive/Pause/core/damaged-build62 remain uncertain/untouched. Independent
k-b status9 applies2033, not this correction; same assessor is parent coordinated.
