# Mounted FAT12 correction — 2026-11-06

Successor to engine10 candidate e21978e7625abea21560043afb6649e0b6468b02,
tree96881f39e656e694cd93654cb985c81d2a3386ed. AH86/Pause/font/history preserved.
Author evidence only; same k-b affected assessment remains parent coordinated.

Two distinct failures: stopped Dos capture inherited global Opts geometry from
preceding BIOS capture; fresh DOS then BIOS reconstruction separately opened the
same lock-mounted image, so the second fopen_lock failed flock. Capture now
clears complete per-record options/allocation padding and uses invocation-local
Opts. BIOS shares the exact-path/geometry DOS-owned native image via original
retained attachment. No raw pointer serialization or disabled locking.

After successful attachment a real INT13 continuation exposed a third issue:
attachment sets imageDiskChange, returning AH06/root99 on first read. BIOS records
now preserve/validate the pending change boolean in their unused mediaid byte,
restore it after attachment. Engine11 separates changed semantics from engine10;
representation9 remains. No guest retry/advance/rollback or weaker unchanged-owner
assertion. Failure still disposes fresh target without evaluator/source mutation.

Changed source: src/dos/dos_files.cpp and src/misc/savestates.cpp. Tests:
fresh-root-matrix.py now includes floppy-held by default; fresh-native-console.py
adds floppy plus missing-medium failure/no held console/no continuation/archive
unchanged, restoring its test-owned medium. Existing fresh-floppy.asm consumes
saved buffer and new INT13 signature read; no feature refusal substituted.

W=/work/dosbox-y/.saivage/work/cards/card-n-c-k-a. Final sequential builds at
W/build/{lua,no-debug}/src/dosbox-x, media-final-{lua,native}.log:
Lua b291a0bac2d5ea996647bea156522eb8f728f9d8a04720526eb49f919a98f18f;
native 170589d61e981c50b9c7ba0bd5d9e5f79563d0ddc7c64c11955cf45c7d5743db.
config.h/config.status unchanged from F2 below. proc-1016bf05a48f passing prefix:
root run-puu9v99c and native-console-id7wm7un repeated held saves/unchanged-owner,
fresh held/delay/save then explicit root42; no-debug/no-Agent/no-Lua native;
mismatch/quit/missing-medium disposal. Subsequent fetch SSH host verification
failed: whole command not pass, no remote effects.

proc-bf9f6c33dad1 passed six cases (floppy/font/AH86/Pause/search/shared-file),
native console and valid-CRC CPU/Memory/Mouse/PIC/late Keyboard/CRC/missing owner
failures, source/attempt unchanged (failure-18vfvld9). proc-a16fc4276e50 passed
four-workload debugger/FILE/INLINE E2E ro60rzdh, missing-medium and Python checks.
Final delta only removes early continue to retain common CD attachment tail and
formats that branch; final builds/floppy tests cover it. No full41 rerun claimed.
proc-fd5679743303 root99 and proc-d48a15fa6820 fixture config-path error remain
real failures corrected before passes. Prior l-b engine10 failures remain true.
Wider composed/devices/font/frontend acceptance and previously unqualified scope
remain parent-owned; no service action, history rewrite or cleanup performed.

# Fresh disposable restore: historical core correction and qualification — 2026-11-06

Author delivery against review R1–R3 (`record:///review.md?card=card-n-c-k&v=1`),
not independent approval. Source is FINAL2033 ancestry plus the correction
committed with this document. Evidence root below is
`/work/dosbox-y/.saivage/work/cards/card-n-c-k-a/` (W). Logs are retained work
evidence, not versioned fixtures. No prior binary is relabeled current.

## Current R1 successor — independent F2 and adjacent core aliases (2026-11-06)

Independent `record:///status.md?card=card-n-c-k-b&v=11` confirms F1 repaired
on492507 but rejects full closure: keyboard controllertype auto/at still rejected
identical input. Source-wide search finds only registration/value list in
src/dosbox.cpp; KEYBOARD_OnReset never reads it. Compatibility now omits this
unconsumed placeholder, **not** the keyboard section or actual controller state.
All spellings remain behaviorally inert; this does not implement XT/PCjr/PC98
controller overrides. Machine remains the actual selector. Aux/reset/type guards,
native input bindings and keyboard bounds/D1/latch codec are unchanged.

Examined required core owners together rather than only this string: CPU and
BIOS conversions above remain; keyboard has precisely aux/reset bools and
auxdevice enum consumed by reset (machine/PCjr/slave-PIC conditions). PIC reset
has another auto decision, cascade-ignore, plus default IRQ delay calculated
from PIT rate. These two now contribute actual reset-owned values through
PIC_StateConfigIdentity(); writes are reset-only, not guest-mutated PIC state.
Memory memalias auto/clamping likewise now contributes MEM_get_address_bits(),
not requested integer. Real address width and PIC policy changes still refuse.
These additional demonstrated equivalences are included in the same regression,
not deferred for serial assessor discoveries. No second parser/validator added.

Other examined core controls have actual consumers: A20 mask/on/on_fake/off/
off_fake/fast selects different gate policy; port92 is a typed bool with actual
handler installation; RAM sizes already use resolved geometry. Keyboard enum
selects actual mouse type; PIC remaining typed bools select cascade/NMI policy;
PIT latch is a typed bool. DOS version/config parsing, files count/FCBs/lastdrive,
SDA/MCB/private memory, XMS/EMS and native layout/service/loaded-input guards
are real consumers rather than unconsumed placeholders. They retain checks;
this audit is not a proof that every possible DOS spelling or architecture-
conditional setting is equivalent or a license to blanket-drop their identity.
Fonts/providers/external devices remain the parent's broader scope. This
successor claims concrete F2 and adjacent core reset equivalences with genuine
mismatch controls, not universal requested-property equivalence certification.

New `fresh-effective-core-input.py`, proc-3b60c8fb7df6,
`effective-core-input-mlryh05i`: fresh producer, auto→at/xt/pcjr/pc98 (all unused)
positives through held save/delay/step/Run; address auto→36, cascade auto→false,
IRQ delay default→838 and combined equivalence. Matching and explicit-at
marker controls pass. Aux=false/reset=false/auxdevice=2button/address32/
cascade=true/delay1000 all refuse Config_Identity exit1/no evaluator/dispose,
source unchanged. Reduced config-identity extraction also passes.

Sequential Lua/native builds and exact hashes proc-3b60c8fb7df6, logs
W/build/core-input-{lua,native}.log:
- Lua f16ac5e98cea3b38453e24a19c59195f3e05229ad12ba9d1f7fc0b89f97c9b8d
- native86ad213f3f96d9ced597b1ede10b432768175e987f74da5009d4caae03d7a695
config.h/config.status profiles unchanged below; previous mutable binaries now
historical. proc-6ef2267378c9 passes BIOS/title regression effective-bios-w2hhgrgx,
native SB no-Agent/no-Lua native-console-3800iwaf, and all33 Lua root/native
workload cases fresh-root-matrix/run-sftfcfrk (includes input active/service,
PIT, files/search, XMS/EMS/A20/hooks). Its subsequent fault invocation failed
before testing: missing DOSBOX_TEST_ARCHIVE, not a safety pass. Corrected
proc-02a03bb62558 actually creates matching audio producer then runs combined
valid-CRC CPU/Memory/PIC/Mouse/late Keyboard faults failure-gammfefc with
AUDIO/LUA/CORE_TRUNCATION/CPU_BOOLS/MEMORY_SAFETY enabled: rejects/disposes,
no evaluator/continuation and unchanged archives. Same command passes CPU
identity regression effective-machine-_w9xgukp. No runtime repair for tests.

Independent status11 combined four-workload E2E and retained status10 R2/R3
root/finalizer/deadline/publication/close evidence retain their input identities;
current tests are author evidence, not independent adjudication. Same assessor
should assess this bounded successor. Historical F2 on492507 remains valid.
§7 CLI/Lua/debugger/slot contract unchanged; effective-identity claims are now
qualified by this concrete correction/evidence, not a universal-device claim.

## Prior R1 successor — independent F1 corrected (2026-11-06)

Independent assessor `record:///status.md?card=card-n-c-k-b&v=10` rejected full
R1 closure on f0af4e: a title-only change and BIOS isapnpport auto/true or APM
auto/1.2 rejected identical initialized machines. The CPU-only closure language
below was insufficient; keeping independent-owner *requested* checks did not
fulfil the initialized-owner requirement. That finding remains valid for f0af4e,
not retrospectively erased by this successor. R2/R3 independent bounded evidence
in that same record remains applicable where owning primitives are unchanged.

`BIOS_StateConfigIdentity()` in `src/ints/bios.cpp` now contributes original
initialized native variables, not config reparsing: integration-device/PnP,
ISA PnP BIOS/port, APM presence/PnP/allowed connection modes/version/power-button
policy, ISA holes and default IRQ handler policy. `savestates.cpp` removes only
these owning properties from requested identity and contributes this value.
`[dosbox] title` is excluded as host presentation, not guest hardware. Strict
loaded-ROM/native roles/slots/spans and genuine machine guards remain.

Proportionate nearby disposition: both BIOS CPU-section auto decisions were
confirmed at ISAPNP_Cfg_Reset; integration-device forced enable for DOSBoxIG is
also represented by the actual variable. DOSBox-section ISA holes resolve
aliases/auto and address-bits/architecture overrides; default IRQ method resolves
architecture default. These now use the same BIOS owner contribution. Other
nearby bools are typed bool properties, already canonical, and integers are typed
values (e.g. dos mem limit); no demonstrated alias cause justifies deleting their
guards. Keyboard aux/reset are typed bools; auxdevice is explicit enum selection,
not the BIOS auto/version parser. Arbitrary fonts/providers/peripheral equivalence
is not claimed solved. This is no new all-device gate or parallel metadata engine.

`fresh-effective-bios.py`, proc-8e7bf077e582,
`effective-bios-s_1n1o94`: new actual producer; PnP auto/true, APM auto/1.2,
ISA-hole auto/false and IRQ default/simple positives through fresh held save/
delay/step/Run. Matching and title-only evaluator controls pass. PnP false,
APM1.1, disabled protected connection mode, ISA hole true, cooperative IRQ and
integration enabled reject Config_Identity exit1 before evaluator, dispose, source
unchanged. Initial `effective-bios-fzb77x1j` tested ISA hole in the wrong section
and integration false (already default); test setup corrected to actual owners/
nondefault values, no guard weakened. Its passing prefix is not a whole pass.

proc-a6d331855da6 passes effective-machine regression `effective-machine-8lqzw6gu`
and debug producer→no-debug/no-Agent/no-Lua `native-console-fau34iu9` with useful
SB FIFO/root42, held status/delay and mismatch/quit disposal. No blanket33-root/
full combined E2E/fault/deadline rerun: independent k-b status10 supports those
on f0af4e; their execution/restore/close owners are unchanged by this identity fix.

Exact successor binaries W/build/{lua,no-debug}/src/dosbox-x (build logs
`build/bios-{lua,native}.log`, proc-0ab3d8db70d3 then proc-8e7bf077e582):
Lua **e637b0cc56c3d41444707e5e19cd54ece3181dbdf03762f0dcadbd2de97c3893**;
native **e6454809a55cf4b7fdeef67698a913e8e286dd2cc7ef5867832c05b770766814**.
config.h/config.status hashes unchanged from the profiles recorded below.
Extraction config-identity and diff-check pass; extraction is not an independent
integration test. Independent affected reassessment must use successor source
and binaries, not the historical f0af paths/hashes. No new independent approval
claimed. §7 interface policy below unchanged; its R1 closure is qualified by
this actual F1 correction and remaining parent scope, not CPU-only evidence.

## R1 — effective initialized CPU owner (f0af4e input)

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
