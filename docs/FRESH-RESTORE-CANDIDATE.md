# Fresh startup restore candidate: author handoff (2026-11-04)

This is an engineering candidate, **not accepted integration or deployment**.
Governing design: `/work/dosbox-y/docs/PLANS/FRESH-TARGET-RESTORE-DESIGN.md`
SHA256 `7204d48e1ab8212c7fb7cb19072786f0bd7022eda0db18122e0f50ab49be23ba`.
Historical r2/Astra remain applicable except the design's expressly superseded
used-target preservation/global preparation/live reload/replay prescriptions.

## Current bounded author receipt

Combined production vertical committed as
`448596413d47407283564b50684d3876fa8b7dc9`, tree
`f73db16cf98aba8a0449d0cf58f9925222e25116` (89 files; owning codecs, original
native constructor seam, startup install, typed root unwind, host pacing,
fixtures and reduced consumer probes). Earlier native-role/no-replay receipts
below are retained as intermediate inputs, not current binaries. Production
is committed, not accepted as whole-brief complete.

Exact committed source/header/config/build/binary/14-fixture receipt:
`/work/dosbox-y/.saivage/work/cards/card-n-c-k-a/build/nov4-committed-fresh-startup-manifest.json`,
SHA256 `e3fe80e059d3b93d5e1f1039e42cb46a232170d7d21110c36938b2598302eaf9`.
Lua `989d405dd2488fc76a56d58d65b375b592a677d919ca852507108edd1392fd6c`,
no-Agent `92c5d9325a0be37e32d764bf1d2e8514dd3a5e9dd9c60bfc0326fcb93688c985`,
no-Lua `af0e031795befca2a3dd10e32457079539971dfd8010db8f22d4d996154b6c7d`.
All3 rebuilt after pacing; matrix `run-xqmx7xvd` and actual no-replay guard
passed. Boundaries/unqualified scope remain below. Documentation and CLI help now follow that combined vertical. The historical
receipt above predates the CLI-help rebuild; it must not identify the current
binaries. The current receipt and current bounded checks are recorded in the
card status.

Current fixture matrix additionally captures active speaker/PIT2/port61, BIOS
keyboard ring, and text video mode/VRAM **before** import; no post-import setup
recreates those witnesses. All18 cases × all3 configured binaries passed in
`fresh-root-matrix/run-t48ngpwc` (`proc-bae231fe153e`). This proves the asserted
native consumers, not waveform/frame/FIFO or all-hardware parity.
The dated author successor is `docs/PLANS/FRESH-RESTORE-R2-SUCCESSOR-20261104.md`;
it is not independent acceptance or adoption of revised product guarantees.

### Historical native-binding slice

Native-binding slice `e2b483eeac05715493f3e17e824e923dbd901554` checks actual fresh
host handler roles as well as portable slot/type/location/body metadata in the
shell/XMS/EMS owners. No host pointer is serialized as identity. Reduced tests
replace every represented host handler with null/wrong roles and require both
capture and consumption failure; legitimate IVT/XMS hook prefixes remain guest
payload. Other core work at that historical receipt was dirty; this slice was not the combined vertical.

Exact source/header/config/binary/14-producer fixture receipt:
`/work/dosbox-y/.saivage/work/cards/card-n-c-k-a/build/nov4-native-roles-source-config-binary-manifest.json`,
SHA256 `535074143cf09d1c6386de5b22976dab6a31ee288dfc5dcbc506ff5f3d8fe63c`.
It includes dirty source and later test-only role substitutions, not a clean
whole-candidate commit. All3 heavy-debug builds passed the 14-case matrix
`fresh-root-matrix/run-f7palog1`, including matching search addition/deletion,
and current malformed-owner/CRC/root disposal consumers. Lua FILE/INLINE
terminal and full-install source-close-error consumers passed. Native service
fixtures exercise explicit continuation, not pre-capture active-device parity.
This receipt supersedes presentation/ingress binary hashes, not historical
applicability. This handoff update does not complete §7 documentation adoption.

## Actual entry points

### Explicit-continuation pacing correction

The saved-state codec correctly avoided sampling host time, but explicit startup
continuation still inherited bootstrap host pacing. `DOSBOX_HoldFreshPacing`
now arms a one-shot flag without guest/host-clock effects;
`DOSBOX_ContinueFreshPacing` rebases only host tick accounting after an actual
native request activation (both local Lua boundary and shared dispatcher).
Saved CPU cycles/PIC/TIMER are not touched. Extracted owner/hold/one-shot tests
pass. All3 builds passed in `proc-10d1333a2884`; 14×all3 matrix
`fresh-root-matrix/run-xqmx7xvd` and no-replay guards passed in
`proc-c21de7950191`. Lua FILE/INLINE hold/terminal/close passed in
`proc-4034274cf993`, whose subsequent malformed invocation **failed** because
its missing AUDIO flag mismatched saved hardware (metadata refusal, not owner
coverage). A subsequent attempt omitted required ROOT_PSP and failed setup.
Correct configured malformed suites `failure-hfpc77rl`, `failure-fcu3he9u`,
`failure-m0g_9p0b` passed (`proc-05b819b47759`). Separate ROM path/changed-byte,
root/helper/CPU/memory checks passed `proc-fa4b0d33a3a4`; all17 owner/transport
extracted checks passed `proc-30b5d14b94e2` (bounded, not independent approval).

No-debug now builds after `ab17373b85f5af509de31bb4c8dcc605798468d6`
fixes a preexisting native-input debugger linkage. Cross-heavy-debug import
failed Shell bindings before the expected frontend refusal, so no cross-debug
compatibility or no-debug root runtime claim is made. Always-built native
headers are now included outside the debug guard. No-debug logs are
`build/no-debug-{configure,make,link-fix-make,restore}.log`. No-debug was rebuilt
after pacing/help (`build/no-debug-current-make.log`); matching-config import
still fails Shell bindings (`build/no-debug-matching-diagnosis.log`), before
frontend refusal. The first new diagnosis used a nonexistent config and failed
Config_Identity; it was a setup failure, not the matching-config witness.
No-debug continuation/root runtime remains separately unqualified.
A matching-config cross-heavy-debug archive currently differs at least in shell
callback slots: producer 74/75/76 versus no-debug fresh target 73/74/75.
`DEBUG_Init()` allocates a debugger callback before native construction, so
frontend build choice currently changes native slot identity. Refusal is correct
for those mismatched bindings; it is not proof that same-layout no-debug root
continuation works, nor permission to remap callbacks or omit binding checks.
`program-root-unwind.py` compiles the actual PROGRAMS_Handler with a polymorphic
host-only double under ASan/UBSan: RootExit retains exact terminal facts and
skips deletion/return; ordinary throw and return delete exactly once. The
integrated polymorphic DOS_Shell-root consumer remains owed.


### No-replay constructor correction (2026-11-04)

The earlier stopped-state receipts did **not** prove constructors were guest-free.
The new actual GDB probe `tests/original-foundation/fresh-startup-no-replay.py`
found two real bootstrap guest calls: DOS external-driver classification polled
CON status (INT16), and shared DOS clock initialization executed INT1A. Native
external-driver type classification now avoids status queries (commit
`a8469b83013932c7d73d3f42d6d7b99a3762d81f`, regression
`264448d98`); fresh construction skips BIOS date sampling because the saved DOS
owner supplies date. Ordinary boot retains date sampling. The latter change is
included in combined commit `448596413d47407283564b50684d3876fa8b7dc9`,
not a standalone clock-only commit.

`proc-65b76358049e` and stronger guard `proc-064202b83e84` reached startup hold
on no-Agent/no-Lua/Lua without RunMachine/normal decoder/PIC queue/TIMER/guest
callback entry. The actual first failures remain in `no-replay-shzsrsuk` and
`no-replay-sy6_q9fv`; these were defects, not success witnesses. Scope is the
configured matching hardware, not all constructor configurations. Current
14×all3 continuation matrix is `fresh-root-matrix/run-ho8wune1`
(`proc-037825295b33`); current malformed suites are `failure-n543lmwp`,
`failure-m55uamom`, `failure-ae1xip_x` (`proc-064202b83e84`). Lua held/terminal/
source-close-error consumers also passed on rebuilt inputs. The preceding
native-role manifest is now historical for binary identities; a new exact
combined source/header/config/binary receipt is still required.


Checkout `/work/dosbox-y/original-savestate-restart`, branch
`original-savestate-restart`. `include/dosbox.h` exposes
`SaveState::restoreFreshStartup(const std::string &path) const`, implemented in
`src/misc/savestates.cpp`. It is startup-only, one attempt per process, outside
machine frames. It constructs native BIOS/DOS/shell/services through
`VM_ConstructFreshNative()`, installs through the common owner traversal,
closes the ZIP, and validates the installed current PSP. Failure terminates the
disposable process; the archive is not modified. Do not call it on a live target.

CLI: `dosbox-x -conf MATCHING.conf -restore-state /absolute/source.sav`.
The main startup seam precedes debugger and Lua. Debug builds hold immediately;
explicit STEPIN/Run is required to execute. Non-debug builds currently refuse
because they have no implemented explicit-continuation frontend.
Native machine entry selects the immutable restored PSP against its actual
machine-owner token, before attachment or dispatch. `DOS_Terminate` checks this
root before its parent/self-parent/status/vector/file/free/stack suffix and
throws an always-built typed unwind to the outer main invocation.
`PROGRAMS_Handler` propagates this unwind without deleting a polymorphic native
program: a shell destructor could otherwise free PSP memory/close guest files
before the outer invocation disposes the process. Ordinary exception cleanup is
unchanged; root departure deliberately leaves that object to process reclamation.

With configured Lua support, add exactly one `-lua-driver-file FILE` or
`-lua-driver-code CODE` for the worker frontend. `require('dosbox')` remains the
native message adapter; standard Lua libraries are opened. FILE/INLINE worker
observation and host delay have author tests, not independent acceptance.
Do not combine with `-lua-driver-program`/`-lua-driver-state` or legacy
`-debug-lua-script`: fresh CLI explicitly rejects these replay/pre-invocation
combinations. FILE/INLINE worker is the supported fresh Lua route.

All live load adapters are intended to refuse fresh-launch-required; final ingress
audit remains outstanding. The Lua `dbg.live.checkpoint{op='load',id=...}`
projection now refuses before native owner/resource inspection, image lookup,
attachment or lease acquisition, even when an image exists. It no longer carries
a dormant install/rebase/stop path; memory capture remains save-only. The actual
adapter extraction test `tests/machine-service/checkpoint-projection.py` checks
this refusal and save/result-allocation lease ordering with mocked native authority.
Repeated stopped STATEWRITE and slot selection/info/
deletion are retained. No automatic guest program launch is needed to import.

## Builds and reproducible witnesses

Card work root `/work/dosbox-y/.saivage/work/cards/card-n-c-k-a` contains
`build/no-lua`, `build/lua`, and `build/no-agent`. Generated inputs and binaries
are evidence, not versioned runtime deliverables. Run `./autogen.sh` before
configuring: inherited configure did not recognize debug-lua.

Lua configuration: `--enable-debug=heavy --enable-dosbox_agent --enable-sdl2
--disable-opengl --disable-avcodec --enable-debug-lua`.
No-Agent configuration substitutes `--disable-dosbox_agent --disable-debug-lua`.
No-Lua Agent uses `--enable-dosbox_agent --disable-debug-lua`.

Tests use `SAIVAGE_CARD_WORK_ROOT`, `DOSBOX_TEST_BINARY`, and producer fixtures:

* `python3 tests/original-foundation/stopped-save.py`
  prints its artifact directory containing `first.sav` and exact layout extents.
  It always checks used-target refusal, repeated held saving/delay, parser errors
  and explicit step. `DOSBOX_TEST_SLOT=1` selects mapper slot save/refusal instead.
  Old `DOSBOX_TEST_LIVE_REFUSAL=1` producer opt-in is no longer required; live
  install/exact-build comparison branches and their obsolete receipt are removed.
* `DOSBOX_TEST_ARCHIVE=.../first.sav python3 tests/original-foundation/fresh-startup.py`
  compares saved guest CPU/RAM and PIC/PIT/input across import and host delay,
  checks repeated saving and explicit instruction continuation.
* `fresh-startup-failure.py` repacks valid-CRC malformed Pic and late Keyboard,
  plus an otherwise consumable late Keyboard with invalid ZIP CRC and an absent
  late Keyboard member;
  checks exit1 and unchanged source/attempt archives; GDB separately attributes
  consumed-owner and transport-settlement failures to common startup installation.
  With `DOSBOX_TEST_FILE_STATE=1`, also changes the archived open-file pathname
  to an absent file of the same encoded length and checks native reopen failure.
  `DOSBOX_TEST_IVT_HOOKS=1` creates a valid-CRC derived input with nondefault
 INT22/23/2E guest vectors, requiring exact held RAM preservation and successful
  explicit continuation. It does not execute those hook handlers.
  `DOSBOX_TEST_XMS_HOOK=1` changes only the five guest-owned XMS hook-prefix
  bytes to NOPs, retaining its native dispatch body; held RAM preservation and
  explicit normal-root continuation must succeed. `DOSBOX_TEST_XMS_SERVICE=1`
  in the stopped-save producer assembles `fresh-xms.asm`: continuation discovers
  XMS via INT2F, calls its far native service through the edited hook prefix,
  validates version >=2.0, allocates16KB, locks/unlocks/releases its handle,
  then exits42 (99 means failure).
  `DOSBOX_TEST_EMS_SERVICE=1` assembles `fresh-ems.asm` to query native INT67
  version>=4.0 and page inventory, allocate/map one page, get its frame,
  write/read A55A, and release before root42. These exercise new allocations
  after restore, not captured nonempty handle reconstruction.
 `DOSBOX_TEST_LIVE_REFUSAL=1` first attempts STATEREAD into the restored held
  target, requires ERR/fresh-launch text, then proves repeated saving/unchanged
  held state and explicit continuation still work.
  `DOSBOX_TEST_BINDINGS=1` also mismatches producer binding metadata in BIOS,
  shell, XMS and EMS, verifying actual owner-attributed startup refusal/disposal.
  It also corrupts native shell, XMS dispatch (after the guest-owned hook prefix),
  and EMS VDMA bodies in RAM with matching binding metadata;
  the respective post-Memory owning consumer must reject the body (GDB x86-64 ABI attribution).
  `DOSBOX_TEST_CORE_TRUNCATION=1` additionally shortens CPU and Memory owner
  payloads with valid CRC, requiring actual owning-consumer fatal rejection and
  process disposal. Each case retains distinct evidence even when another case
  corrupts the same owner for a different reason.
  `DOSBOX_TEST_ROOT_VALIDATION=1 DOSBOX_TEST_ROOT_PSP=0814` on the normal
  fixture also damages PSP signature, its MCB owner, parent, saved stack and out-of-range INT22 vector in valid-CRC Memory;
  the actual Dos consumer must reject after Memory installed, before evaluator.
  The PSP argument is the producer's witnessed segment, not a generic assumption.
  `DOSBOX_TEST_METADATA=1` also truncates each current metadata member, proving
  startup disposes even a pre-owner rejection; broad config identity remains a
  known compatibility design gap, not a recommended final contract.
  `DOSBOX_TEST_LUA=1` adds an INLINE evaluator marker (`DOSBOX_TEST_LUA_MODE=file`
  selects FILE) which must remain absent
  in every failed consumer process. This detects evaluation, not attachment.
  Frontend logging can be discarded after SDL initialization.
* Producer and `fresh-startup.py` accept `DOSBOX_TEST_MEMSIZE` (MB) and
  `DOSBOX_TEST_CYCLES` (fixed cycles) for matching preconfigured consumers.
  `DOSBOX_TEST_AUTO_CYCLES=1` in both producer and consumer instead configures
  `cycles=auto`. A terminating fixture (`DOSBOX_TEST_PROGRAM_HEX='90 b8 2a 4c cd 21 eb fe'`)
  with AUDIO=1 passes repeated held saves/host delay, exact CPU/PIC/Dosbox-x
  comparisons, then explicit step/Run/root42 across all three configured builds
  (`proc-21fd94a2bffa` / `proc-71ab282797db`). This is a bounded host-delay
  rebudget/catch-up witness, not every cycle algorithm or long-running workload.
  Author runtime witnesses include 16MB/3000 and 32MB/7000, not a fixed whitelist.
  `DOSBOX_TEST_PRESENTATION=1` on the consumer changes scaler/aspect/ratio/frame
  skipping without changing guest hardware. These four render properties are
  no longer compatibility identity; raster/font switches remain conservatively
  constrained pending effective-owner identity. Capture slot/file/remark,
  autosave policy and the inert legacy force-load preference are excluded from
  compatibility: changing them must not change initialized hardware identity.
  `DOSBOX_TEST_CAPTURE_POLICY=1` exercises different consumer preferences in the
  fresh matrix; `config-identity.py` checks the actual traversal with reduced
  hash/section doubles and retains a memory-size distinction. A disk-audio/Xvfb producer to
  dummy-audio/video consumer passes held saves and explicit root continuation
  across all three configured builds with emulated sound enabled. This is not
  waveform/frame parity or completion of the broader §3 identity replacement.
* INT10 capture/import validates the actual fresh handler role and encoded
  native CB_IRET body after dependent Memory installation, without constraining
  legitimate guest IVT hooks. Valid-CRC RAM body damage must fail at Int10.
* CPU import checks the raw pmode/stack.big/code.big/trap_skip bool bytes
  before copying CPUBlock, avoiding invalid-bool evaluation even on disposable
  failure. Actual valid-CRC owner mutations reject at CPU in all three builds;
  reduced decoder consumer covers these representations under ASan/UBSan.
* Paging capture emits deterministic zero bytes for the existing discarded host
  TLB/cache-link wire extents rather than serializing host addresses. Import
  still rebuilds native caches. A real matrix attempt exposed first→second CPU
  cache-byte drift (not guest advance) before this repair; unchanged second→third
  archives did not erase that failure. The corrected 18×all3 matrix is separate.
* Slot/hotkey/autosave entry now checks the native frontier before focus/key/UI
  work (and direct `SaveState::save` before its remark dialog), not just before
  archive publication. `autosave-frontier.py` executes the actual entries in
  debug/no-debug modes with reduced UI doubles and checks no denied-path effects.
* Restored FILE/INLINE probes exercise real C `require`, `dofile`, Lua errors,
  held repeated saving/host delay and successful process close. Set
  `DOSBOX_TEST_BLOCKED_FINALIZER=1` on fresh-startup-terminal.py for a real C
  finalizer block: terminal response/revoked lexical close precede the block;
  process remains owned until invocation-local release, then revoked GC receipt
  and disposal complete. This is not a host transport/frontend close race proof.
* Mouse import checks raw bool object representations before evaluation and
  rejects nonfinite position/sensitivity/accumulator/timestamp values at the
  owning codec. `DOSBOX_TEST_MOUSE_SCALARS=1 fresh-startup-failure.py` adds real
  valid-CRC malformed callback bool imports; mouse-consumption.py also exercises
  actual-struct bool fields and NaN/infinity under UBSan. This is not complete
  mouse native callback identity or physical-input/PC98 qualification.
* `python3 tests/original-foundation/checkpoint-stream.py` checks bounded common
  memory capture and failure publication, plus unconditional direct file/slot
  and checkpoint used-target refusal without invoking any owner installer.
  `progressive-install.py` separately covers actual common startup install
  traversal failures with reduced transports; real disposable startup and late
  owner failures are exercised by fresh-startup-failure.py.
* `python3 tests/original-foundation/state-departure.py` captures an ordinary
  terminating program, refuses used-target STATEREAD, compares repeated saved
  owners unchanged, then explicitly RUNs the program back to its ordinary shell
  and verifies its RETURN.TXT marker and clean process exit. No restored-root or
  same-process reload support is inferred; fresh-startup terminal probes own
  disposable-session exit coverage.
* `python3 tests/original-foundation/mouse-consumption.py` extracts the actual
  mouse struct/codec with reduced transport doubles: default/user/NEC mask roles,
  every short stream, invalid roles, event/AUX/unlock bounds. The owner clears
  serialized pointer fields before any consumer can use them and emits no mask
  host addresses. Revision7 PC98 mouse suffix is mandatory; short input remains
  failed instead of clearing EOF as legacy optional suffix. Actual fresh-failure
  probes cover valid-CRC invalid mask roles and one-byte-short mouse suffix.
  This does not qualify complete mouse native callback bindings or PC98 hardware.
* `DOSBOX_TEST_AUDIO=1` in producer and consumer keeps emulated audio enabled.
  Producer uses Xvfb software surface; consumer SDL dummy surface/audio.
  Default producer host audio is dummy; select actual disk output with
  `DOSBOX_TEST_AUDIO_DRIVER=disk` for the positive distinct-audio-driver witness
  described below (nonempty sink is not waveform fidelity).
* `DOSBOX_TEST_DBOPL=1` in fresh-startup-failure adds valid-CRC invalid DBOPL
  volume, wave-offset and synthesis roles plus six LFO index/shift/divisor/counter
  bounds to actual startup refusal/disposal.
  `DOSBOX_TEST_CONFIG_DIR=/configured/build python3 tests/original-foundation/dbopl-consumption.py`
  compiles the actual whole DBOPL with ASan/UBSan for bounded marker, short
  stream, role, wave alignment/span checks and native generator roundtrip.
  This does not qualify all raw chip scalar consumers or other OPL engines.
* Saved active searches: producer `DOSBOX_TEST_SEARCH_HELD=1` with capture IP
  `0814:0200` and refusal registers `CS:IP=0814:0202` captures two simultaneous
  classic DTA searches after FindFirst. ROOT_EXIT consumer STEP_IP0814:0202 must
  continue both independently, return the distinct remaining .DAT and exhaust18.
  The original localDrive codec now records native slot/directory/position and
  last-returned short-name anchor, rebuilding from current directory contents
  via native FindFirst (not guest INT21 or restoring host entries). If the anchor
  vanished, position is bounded to available entries; no historical enumeration
  snapshot is promised. LFN global-table searches are explicitly unrepresented
  and refuse saving, not silently encoded as classic DTA searches.
  Consumer `DOSBOX_TEST_SEARCH_CHANGED_ENTRY=1` adds an unrelated host file
  before import and verifies it survives. `DOSBOX_TEST_SEARCH_MATCHING_ENTRY=1`
  in producer and consumer instead adds `ZZZ.DAT` after capture: both DTA searches
  must read its exact name before exhaustion18 and preserve its host bytes.
  `fresh-root-matrix.py` includes this current-content case across all selected
  consumers. LFN refusal witness: producer `DOSBOX_TEST_LFN_HELD=1`
  `DOSBOX_TEST_CAPTURE_IP=074B:0200` enables DOS7.1/LFN and runs the real AX714E
  service before capture, requires save ERR/no publication, then explicit NOP
  step to074B:0201. Segment is this configured fixture's witnessed PSP, not a
  general LFN identity. Failure probe `DOSBOX_TEST_SEARCH_STATE=1`
  damages represented search tag/path/truncation with valid CRC, requiring owning
  DOS drive refusal/disposal and unchanged archives.
* Shared file fixture: producer `DOSBOX_TEST_SHARED_FILE=1`,
  `DOSBOX_TEST_CAPTURE_IP=0814:0200`,
  `DOSBOX_TEST_REFUSAL_REGISTERS=CS:IP=0814:0204` saves duplicated read-only
  handles. Fresh ROOT_EXIT consumer with STEP_IP0814:0204 checks alias cursor
  sharing and survival after closing one reference against changed host content.
  It is also included in fresh-root-matrix; active search/timestamps are separate.
* Saved nonempty resources: producer `DOSBOX_TEST_XMS_HELD=1` or
  `DOSBOX_TEST_EMS_HELD=1`, with `DOSBOX_TEST_CAPTURE_IP=0814:0200`, initializes
  a locked 16KB XMS handle/A55A payload or a mapped EMS page/A55A before capture.
  `DOSBOX_TEST_REFUSAL_REGISTERS` is `CS:IP=0814:0202` (XMS) or
  `CS:IP=0814:0207` (EMS). Pass the resulting archive to fresh-startup with audio,
  ROOT_EXIT and matching STEP_IP; actual service readback/unlock/remap/free must
  reach root42. Capture setup runs only on producer; no consumer guest replay.
* `python3 tests/original-foundation/fresh-root-matrix.py` reproduces child return,
  custom INT22, INT27, AH31 and self-parent root cuts using fresh archive consumers.
  `DOSBOX_TEST_BINARY` selects producer; optional `DOSBOX_TEST_CONSUMERS` is an
  `os.pathsep`-separated list of compatible binaries (e.g. no-Agent/no-Lua/Lua).
  It clears inherited probe fixture/oracle overrides and records all subprocess
  evidence under `fresh-root-matrix/`; failed probes stop the matrix.
* `fresh-startup-lua.py` exercises FILE and INLINE with full libraries, worker
  saves around host sleep, and a Lua close finalizer. Consumer requires an
  audio-enabled same-binary producer archive.
* `fresh-startup-terminal.py` consumes a normal-root archive with FILE and INLINE,
  checks exact copied terminal facts, revoked context/no continuation, lexical
  close and a globally reachable `__gc` object at actual lua_close; context access
  from both finalizers must refuse. This is not blocked-C or finalizer-error coverage.
* A producer `DOSBOX_TEST_PROGRAM_HEX='90 cd 19 eb fe'` followed by consumer
  `DOSBOX_TEST_NATIVE_DEPARTURE=1` proves held import/host delay then explicit RUN
  refuses native reset signal3 with exit1, rather than ordinary BIOS reboot fallback.
  The same refusal applies to selected restored-session BOOT/reboot signals
  2/3/6/7/8/9; other individual signal paths still need qualification.
* `DOSBOX_TEST_PROGRAM_HEX` supplies small faithful COM bytes to producer;
  `DOSBOX_TEST_ROOT_EXIT=1` and `DOSBOX_TEST_ROOT_FACTS` in fresh-startup assert
  actual root normal/TSR/self-parent process exit and logged exact terminal facts.
  INT27 fixture `fresh-int27.asm` (`DOSBOX_TEST_INT27=1`) emits
  `90 ba 00 02 cd 27 eb fe` and resizes resident allocation via
  its ordinary service prefix; expect `code=0 resident=1 self-parent=0`.
  FILE/INLINE terminal probe accepts `DOSBOX_TEST_EXIT_CODE=0` with
  `DOSBOX_TEST_RESIDENT=1` for this service (default exit expectation remains42).
* `DOSBOX_TEST_FILE_STATE=1` in producer and fresh-startup consumer captures an
  actual open read-only file at cursor3, changes host contents between processes,
  then explicitly continues reads/cursor/write denial and new current-directory
  enumeration (including exhaustion error and exact names). It does not claim
  restored active searches, shared-handle identity or timestamp qualification.
* `DOSBOX_TEST_EXEC_CHILD=1` selects `fresh-child.asm`: root moves SP into its
  retained allocation, shrinks its block,
  EXECs a newly loaded child exiting7, checks AH4D status, then exits42 (failure99).
  Use `DOSBOX_TEST_ROOT_EXIT=1` for consumer. Thus root42 also proves child return.
* `DOSBOX_TEST_CUSTOM_INT22=1` assembles `fresh-int22.asm`; explicit continuation
  installs custom PSP INT22 pointing to an unsafe print/infinite-loop suffix,
  then exits42. Consumer requires process disposal with no unsafe marker.
  To capture already-custom INT22, use three producer capture setup steps,
  expected `CS:IP=0814:010C AX=0814 BX=0000`, and consumer step IP `0814:010C`.
* Capture already-self-parent root: producer `DOSBOX_TEST_PROGRAM_HEX='8c c8 a3
  16 00 90 b8 2a 4c cd 21 eb fe'`, `DOSBOX_TEST_CAPTURE_STEPS=2`,
  `DOSBOX_TEST_REFUSAL_REGISTERS='CS:IP=0814:0106 AX=0814 BX=0000'`.
  Consumer `DOSBOX_TEST_STEP_IP=0814:0106`, `DOSBOX_TEST_ROOT_EXIT=1`,
  `DOSBOX_TEST_ROOT_FACTS='code=42 resident=0 self-parent=1'`.
  Setup steps are authorized guest execution before capture, not restore replay.

* `DOSBOX_TEST_SPEAKER=1`, `DOSBOX_TEST_VIDEO=1`, or `DOSBOX_TEST_KEYBOARD=1`
  assembles the respective small faithful `fresh-{speaker,video,keyboard}.asm`.
  Speaker programs PIT2/port61 and waits two guest BIOS timer ticks; video checks
  mode3 VRAM/INT10 readback and guest RAM; keyboard checks INT16 peek/read and
  BIOS ring head consumption. Each exits42 or failure99 after explicit execution.
  These are not waveform/pixel/host-key IRQ parity qualification.
* Producer `DOSBOX_TEST_AUDIO_DRIVER=disk DOSBOX_TEST_AUDIO=1` requests real SDL
  disk audio (requires actual backend log plus nonempty card-owned host sink).
  Consumer still uses dummy audio/video with identical emulated configuration.
  Whole-Mixer cross-process equality is not an oracle: legacy reconstructed OPL/SB
  native addresses differ. Held one/two-save equality still covers no host-delay
  mutation; semantic device continuation needs workload assertions above/beyond.
* `tests/original-foundation/fresh-startup-close-failure.py` builds its small
  Linux `source-close-failure.c` interposer, closes only the exact source stream
  then reports EIO, and proves full install cannot publish/attach/evaluate after
  top-level archive close failure. Vendored `unzClose` now propagates native close
  status instead of silently returning UNZ_OK. Source bytes remain unchanged.
  GDB asserts `installed=true` before accepting the witness; stale metadata
  refusal is not evidence of native close failure. `minizip-close.py` separately
  runs actual minizip/ioapi with a failed native close and a pending bad CRC,
  preserving member error priority and exact one-close accounting.
* `DOSBOX_TEST_FINALIZER_ERROR=1` in the terminal probe raises from lexical
  `__close` and reachable `__gc` during actual lua_close after recording revoked
  context. Expected native exit is1 (not success), root terminal facts remain
  exact and both refusal receipts must survive. Ordinary mode expects exit0.
* BASIC ROM compatibility: producer `DOSBOX_TEST_BASIC_ROM=1` rewrites path
  contents after initialization and requires unchanged loaded-input identity.
  Fresh consumer accepts `DOSBOX_TEST_BASIC_ROM_PATH=/different/path` with same
  loaded bytes. Failure probe `DOSBOX_TEST_CHANGED_BASIC_ROM=1` requires rejection
  for freshly loaded different bytes. ROM path names are not input identity.

## Known nonacceptance and next integration boundary

Revision7 (engine7; active-search codec added, no rev6 migration) uses a versioned common-owner engine/ABI identity rather than exact
executable equality: byte order/scalar widths/alignment/C++ ABI and FPU/decoder
features are compared. Build hashes remain evidence. One latest Lua producer
archive now actually restores through Lua, no-Lua and no-Agent binaries, held
through observation/delay/saves before explicit root42 continuation. Owner-specific
ABI extents and overly broad resolved config identity remain incomplete: this
is **not full §3 semantic compatibility**. Do not advertise general cross-platform
or backend-config compatibility. Initialized immutable-input/native
binding coverage, full root terminal frontend settlement/close,
R3 resources/search, real audio/timer/input workload, other native
resource failures, and final all-ingress audit remain obligations. Stopped native
control now uses full coordinator ancestry and exact restored invocation rather
than fixed selected-root numeric depth; this has unit and all3 real root-matrix
author witnesses, not general frontend qualification. Lua live checkpoint load
refuses before native authority/resource/image inspection; its extracted adapter
test mocks authority and is not a real legacy-live dispatcher runtime witness.
Root child return,
custom INT22 and saved-self-parent have bounded author runtime witnesses, not
exhaustive termination qualification. Full no-debug
operation is not implemented. MIDI remains explicitly unqualified lower priority.

No deployed service or release was changed. Author tests are not independent
assessment. Consult the card's current status for exact binary/evidence identities
and outstanding work before adopting this candidate.
