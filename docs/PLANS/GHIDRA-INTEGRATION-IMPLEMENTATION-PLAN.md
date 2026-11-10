# Ghidra integration — staged implementation plan

Date: 2026-11-10. Revision: Lua plan r3.
**Status: approved for staged implementation. No implementation has begun.**
This document is a work decomposition, not evidence
that a prototype, package, runtime or release has passed.
R3 retains SOL-PLAN-1's R2 correction and addresses the verified DeepSeek R1
findings. Approval is based on Sol's independent R3 approval plus the author's
source-grounded review and final clean-room read, under the owner's Sol-only
closure decision. No GLM or DeepSeek R3 approval is claimed: their non-NVIDIA
providers are unavailable, and NVIDIA is explicitly untrusted and excluded.

## 1. Frozen authority and execution rules

Repository: `dosbox-y/original-savestate-restart`, HEAD
`52107ea55ab23319272cb5b13a920fd624ba23d1`. Authorities at that HEAD:
[analysis](GHIDRA-INTEGRATION-ANALYSIS.md),
[functional specification](GHIDRA-INTEGRATION-FUNCTIONAL-SPEC.md), and
[technical design](GHIDRA-INTEGRATION-TECHNICAL-DESIGN.md). Parent
`dosbox-y/AGENTS.md` and current `dosbox-y/TODO.md` govern engineering and scope.
Their older working-input notes are provenance, not instructions to revive old
controllers. Existing untracked test `__pycache__` directories are unrelated.

Fixed chain: Ghidra 12.2 → external Lua Trace RMI host → inherited FD3 → sole
managed Lua worker → singular asynchronous native operation core/exclusive owner.
No Python product runtime/build helper/generated client, Agent RPC carrier or
fallback, protocol probing, second evaluator/controller/CPU loop, compatibility
engine, generic transport framework or hidden synchronous execution path.
Existing Python regression drivers remain independent tests, not dependencies of
the installed integration. Preserve supported unrelated frontends/input/capture.

Before each implementation changeset, refresh HEAD, affected source and actual
caller inventory against this baseline; resolve relevant drift before mutation.
Do not silently use a concurrently changed checkout or infer deployment authority.
All proposed new paths below are marked **new**; names may be refined to fit actual
build conventions without changing ownership. Existing paths name likely edit
owners, not a requirement to modify every listed file.

Each package below specifies objective/delta, source ownership, dependencies,
requirements, tests/evidence, documentation and non-goals. A package is one coherent
validated commit unless its listed inseparable subparts require one combined commit.
Do not commit broken intermediate migrations. Tests and directly affected docs ship
with their producer/consumer changes; release documentation packages finish the
cross-cutting installation/evidence story rather than defer contract documentation.

## 2. Delivery graph and commit boundaries

| Work package | Prerequisites | Boundary / isolation |
|---|---|---|
| S0-A shared Active/Pause seam | approved plan | Independent native extraction; existing callers remain working |
| S0-B provisional-event ownership | S0-A | Independent lifetime refactor preserving current scripting behavior |
| S0-C singular async core and caller migration | S0-A, S0-B | Native/handoff/worker/live bindings migrate together; no dual engine |
| S0-D FD3 v1 and terminal delivery cutover | S0-C | Dispatcher, teardown, all custom-host clients and docs land together |
| S0-E Lua package/wire/schema foundations | approved plan; S0-D for FD3 integration | Codec/socket tests can be isolated; no advertised debugger yet |
| S0-F Linux child containment | S0-E; S0-D for healthy Close | Independent lifecycle package; complete before actual child carrier proof |
| S0-G Stage 0 installed proof and notices | S0-A–F | A0/A0b/A1 and minimal actual Ghidra wire proof; Stage 0 closure |
| R1-A guarded Java consumer and launch offer | S0-E/F; S0-G before product qualification | Guard/build identity/launcher gate inseparable for enabled effects |
| R1-B native observation/load/hit projection | S0-G | Native + typed producer/consumer evolution together |
| R1-C early actual consumer vertical | R1-A/B | Minimal live COM views/actions; stop/go before full adapter polish |
| R1-D mapping, breakpoints, memory/time completion | R1-C | Small complete consumer slices as below; no partial R1 release |
| R1-E installed R1 acceptance | R1-D | A2–A5, focused regressions and release/help/notices |
| R2a-A MZ delivery | R1-E | Independent A6 release; does not require R2b |
| R2b-A fresh restore delivery | R1-E | Independent A7 release; does not require R2a |
| L-A DOS lifetimes | relevant released mapping baseline | One qualified lifetime increment at a time |
| L-B protected/mixed profiles | R1-E plus approved representation proof | One qualified mode/profile increment at a time |

Critical path: S0-A → S0-B → S0-C → S0-D → S0-G → R1-B/R1-A
→ R1-C → R1-D → R1-E. S0-E and the lifecycle-only parts of S0-F can proceed
independently of native work; their integration joins at S0-G. Java guard source
work can be isolated against the pinned Ghidra tree while native work proceeds.
This is dependency parallelism, not permission for concurrent mutation of shared
files. R2a/R2b are independent after R1; later profiles never gate a truthful R1.

S0-C must include callback adapters and live execution adoption before removing
the old machinery. S0-D must include terminal transfer, non-destructive wake,
dispatch and updated custom-host clients: none may ship alone against the other
contract. S0-C retains a working synchronous public session until S0-D exposes
the new fixed asynchronous wire surface; this is staging one implementation, not
two production execution paths. Any newly exposed internal async binding must
already obey native revocation and existing close semantics; it is not advertised
as the completed departed-delivery contract before S0-D.

Source rollback means a reviewed forward revert of a complete coherent changeset
and its consumers/package, after checking dependent commits. It never means
replaying uncertain effects, restoring an old process/session, downgrading an open
channel, reconstructing runtime state or rolling back guest/trace side effects.
After protocol adoption, a source revert requires matching client and host builds
and newly launched disposable sessions. No runtime compatibility path is retained.

## 3. Stage 0 — smallest coherent foundations

### S0-A — Extract native Active control, including the actual Pending seam

**Objective/delta:** make exact-operation Pause and copied status serviceable in a
managed-Lua build with Agent disabled. Extract the minimum stop submit/consume/body
from Agent ownership; preserve Agent ordinary-effect ordering. Service the control
flag before the condition-free early return in `LocalDriverFrontier`. Pending
status/refusal must not activate the guest. Preserve the held frontier's separate
enqueue → preparing → `DEBUG_DispatchNativeExecution` turn; merely moving an outer
loop check cannot activate work while that frontier still owns the stack.

**Owners/files:** `src/debug/debug.cpp` (`LocalDriverFrontier`, native dispatch and
stop operations), `src/debug/debug_common.h`, `src/dosbox.cpp` (`Normal_Loop`),
`src/agent/bridge/agent_bridge.cpp`, relevant declarations in
`include/agent/agent_bridge.h`, `include/native_debug_execution.h`. Prefer existing
owners; extract a small native service source only if needed, with its header and
`src/debug/Makefile.am`/`include/Makefile.am` wiring in this commit. Keep Agent pump
gated while moving the shared Pending dispatch check to the appropriate debugger
build guard.

**Dependencies/requirements:** approved plan; GI-LIFE-01/02, GI-OP-03/04,
GI-PKG-03. This is service preparation, not completed public async admission.

**Tests/evidence:** extend `tests/machine-service/execution.cpp`, `frontier.cpp`
and `control-owner.cpp` for exact Active stop, stale Pause against successor,
Pending refusal without forced activation, and ordinary Agent command ordering.
Build with Agent off and on. Run a native controlled Pending barrier; don't require
transported Observe to catch Pending. Record relevant hot-loop control-check cost
without promising a latency bound. A0's actual same-channel proof waits for S0-D.

**Docs:** update the native service/Agent-off explanation in the existing tracked
`docs/managed-lua-session.md`; record extraction/build limits with its test recipes.
Do not recreate historical exclusive-native-slice documentation referenced by
parent TODO; it is not a tracked current documentation owner here.
**Non-goals:** moving Agent parsing/catalog/queue into Lua, broad Agent redesign,
new stop conditions, synthetic phases, additional CPU dispatch loops.

### S0-B — Decouple the one provisional event from admission evaluation

**Objective/delta:** retain native hit authority until exact worker verdict or
native error/expiry/Pause/close/departure arbitration; make it independent of
`Handoff::Phase::Executing`. One event carries operation, registration, ordinal and
native invocation/checkpoint. Ordinary stopped reads remain refused at event-pending.
Explicit worker event service alone invokes callbacks; Observe/Wait never does.
Install the named native contract constant `event_service_timeout_ms = 30000` and
native-owned per-event `event_verdict_deadline` (publication time plus that duration).
The approved design §4.4 fixes 30000 ms; it is not implementation-selected or publicly
configurable. S0-B exposes this constant/deadline only to native/worker tests and
current contract documentation; the `ready.event_service_timeout_ms` wire fact
arrives atomically in S0-D, not through the old dependency Ready record.

Keep this distinct from existing `LuaDriver::Request::timeout_ms`: the current
consumer is `Debugger_LocalDriverSave(..., request.timeout_ms, ...)`, not a read
or transport timeout. Preserve its Save option/default behavior; do not reuse it
for verdict expiry or rename public Save options as part of this work. Use a
separate named event constant (with the same required numeric default, not a second
unexplained literal). Neither Save timeout, bounded Wait nor the seven transport
budgets sets or restarts the event verdict deadline.

**Owners/files:** `include/lua_driver_handoff.h` event/read/verdict ownership;
`src/debug/debug.cpp::LocalDriverObserveHit`; worker `observation`, callback guards
and binding dispatch in `src/debug/lua_driver_worker.cpp`. Keep the actual native
hit frame only while the event is pending, not for remote terminal delivery.
The existing synchronous call becomes the first consumer of explicit event service
so this changeset works before async admission is exposed.

**Dependencies/requirements:** S0-A; GI-OP-07, GI-OBS-01, GI-TERM-01,
GI-LIFE-02, GI-FAIL-01/02.

**Tests/evidence:** extend `handoff.cpp`, `worker.cpp`, `worker-native.cpp`,
`stop-conditions.cpp`, and existing `lua-production-runtime.py` callback scenarios.
Prove suppression and propagation, read equals actual held hit state, no guest
progress while pending, close/Pause versus verdict ordering, checkpoint expiry,
late read/verdict refusal and native 30000 ms expiry. Include a blocked foreign-code
callback fixture whose native authority expires without claiming C preemption.
Existing callback/coroutine/effect restrictions must still pass.
Tests assert the actual constant, native publication/deadline observations and
independence from Save request timeout. Use existing native time seams if available
for focused boundary tests; otherwise one real-deadline fixture observes expiry.
Do not add a public timeout override or a clock framework merely to speed tests.

**Docs:** update the existing tracked `docs/managed-lua-session.md`; describe
the distinction between provisional and ordinary contexts and foreign-code limits.
It owns this bounded managed callback/service explanation; no new authority-like
document or nonexistent managed-held-lua document is needed.
**Non-goals:** dropping callbacks, copying tokens to extend stack lifetime,
native-thread Lua, a callback pool, generic event journal, configurable deadline.

### S0-C — One async core; migrate sync helpers and affected dbg.live together

**Objective/delta:** separate exchange completion from retained native operation.
Add Start/Observe/bounded Wait/Pause/exact Release projections around the existing
`NativeDebug::Execution`. Session identity is available at readiness before Start;
revision uses native invocation/checkpoint. Native execution stays emulator-thread
owned; handoff stores copied facts, never callable native pointers on the worker.
Held Terminal persists until exact release; stale/released IDs refuse. Start returns
admission, not completion or guaranteed visible Pending. Activation failure settles
rather than strands Pending. Preserve all raw outcome, context, validity, budget
and continuation facts; unknown raw reasons are unclassified, not guessed success.

**Owners/files:** `include/native_debug_execution.h`, `include/lua_driver_handoff.h`,
`src/debug/debug.cpp` local driver and `Debugger_LiveResume`, `Debugger_LiveStop`,
`Debugger_LivePhaseCompleted`; `src/debug/debug_lua.cpp` live Resume/Stop/Phase/Status
and yielding `LiveSlice`/`LiveFrame`; `src/debug/lua_driver_worker.cpp::invoke/module`
and current helper definitions; current public Live declarations belong to
`src/debug/debug_lua.h` (`Debugger_LiveStop`, `Debugger_LiveResume`,
`Debugger_LivePhaseCompleted` and related observations). `LiveSlice`/`LiveFrame`
remain implementation-local to `debug_lua.cpp`, with no header API introduced.
Do not add their declarations to `debug_common.h`; that header remains relevant
only to genuinely shared native/debugger APIs elsewhere in the plan.

**Adoption in this commit:** (1) route current managed run/step/over/run_until
through Start → bounded Wait → explicit event service → copy → Release;
(2) route live execution bindings through the same native core using yielding
dispatcher continuations, never worker waits inside `LiveSlice`; (3) delete raw
resume/phase operation coordination rendered dead by these replacements; (4) make
breakpoint/manual stops continuable when native facts permit rather than passing
them to the permanent live error latch. Preserve supported live phase/budget
semantics, standalone/interactive restrictions, save bindings, input/capture and
legacy read semantics. Do not route Ghidra reads through `Debugger_ReadMemory`.

**Dependencies/requirements:** S0-A/B; GI-LIFE-01–03, GI-OP-01–07, GI-OBS-01,
GI-FAIL-01/02. No flag switches between old/new engines are allowed.

**Tests/evidence:** native retained-result/refusal tests in `execution.cpp`;
handoff and worker tests; real `lua-production-runtime.py` plus current live
consumer scenarios in `tests/machine-service/` and `tests/original-foundation/`.
Compare async and sync stop meaning, not numeric IDs across separate sessions.
Exercise ordinary breakpoint/manual stop then valid execution, both live and managed
callers sequentially. Audit affected call sites for eliminated independent
execution bookkeeping. Reuse unchanged input/capture evidence; rerun affected
`input-primitives.py`, `bridge-capture.py` or screenshot cases as applicable.

**Docs:** current live/shared contract docs, `docs/managed-lua-session.md`, and
test usage notes describe the single core and synchronous convenience. Record
that the old external session wire is not yet the new asynchronous delivery API.
**Non-goals:** breaking unrelated public reads, spinning synchronous waits in
emulator slices, preserving a second engine for callbacks, promising remote history.

### S0-D — Atomic DOSBox FD3 v1, departure transfer and client cutover

**Objective/delta:** replace this host's `lua_repl_mcp.session.serve` with one
DOSBox-owned strict dispatcher from ready through terminal delivery. Implement the
design §6 union, not a dependency protocol fork: fixed typed live records, fixed
Observe/Wait/Release/Close, separate trusted scripting Execute. All Ghidra requests
select fixed bindings with data, never Lua source or arbitrary function names.
Ready reports allocation generation, `generation:0`, session/revision/capabilities/
limits and S0-B's `event_service_timeout_ms`; Wait is 0..250 ms. Select/use the
unchanged pinned `lua_repl_mcp.codec` strict decoder, which already rejects duplicate
object keys; demonstrate that behavior through the adopted decoder in a focused
fixture (including null/false-valued duplicates). DOSBox validates decoded union,
unknown fields/types, canonical integers, enums, exact generation/session and bounds,
including all seven coupled bootstrap budgets. Do not add another duplicate-key
scanner/JSON parser, copy the dependency codec or weaken its rejection behavior.

**Allocation producer:** the external Lua host (S0-E/F), acting as this single-child
launcher, initializes one local allocation generation to canonical positive `1`
for its fresh host/process/channel lifetime, supplies it via
`LUA_REPL_MCP_ALLOCATION_GENERATION` before exec and retains it for comparison.
There is one allocation per host, no in-process respawn/reconnect or durable/global
generation registry. Java owns the host Process, not a competing allocation counter.
This value is scoped to that fresh owned channel, not globally unique; native
attachment serial joins it in the session identity. Direct S0-D fixtures are the
producer for their fresh fixture-owned channel (currently they also supply `1`);
ordinary dependency manager sessions keep their own existing allocation producer.
The worker decodes the supplied environment through the unchanged pinned bootstrap,
does not mint/replace it, and reports it in Ready and all subsequent records.
Client and worker compare the exact allocation/session on every exchange. Respect
the bootstrap's positive-integer range and convert to the v1 canonical spelling
without floating-point loss; allocation generation is distinct from per-request
generation. Missing/invalid bootstrap fails launch; mismatched records poison the
channel, never authorize a new generation or reuse of an old session.

At native departure settle/copy before owner unwind; revoke all contexts and
registrations, retire native retention once and keep only the delivery copy.
Separate native mode, transport health and worker closure. Serialize live release,
departure and dispatch; preserve an already-decided response. Session-only departure
never invents an operation or release. Keep same FD3/worker for fixed delivery.
Use tagged data/wake/eof/error and a local wake FD while retaining partial framing;
move remote wait/join from ExitFence/restored-root borrowed frames to the top-level
session owner. Release consumes copy → matching reply flush → worker close →
join/resource closure. Failed flush after consumption never permits replay/probe.

**Owners/files:** **new** `src/debug/lua/dosbox/managed_protocol.lua` and
`src/debug/lua/dosbox/managed_session.lua` (source layout matches the installed
DOSBox namespace); `src/debug/lua_driver_worker.cpp`
`managedSession/sessionRead/sessionStopping/runWorker`;
`include/lua_session_transport.h`, `include/lua_driver_handoff.h`,
`include/lua_driver_worker.h`, `src/debug/debug.cpp` ExitFence,
`DEBUG_CloseLocalLuaDriver`/`LocalDriverRetire`; top-level teardown callers in
`src/dosbox.cpp`/`src/gui/sdlmain.cpp` as freshly identified. Keep the specific
NativeBoundary departure settler before unwind, not generic owner-loss substitution.
**Tracked/install/resolution contract:** choose ordinary Lua data installation,
not generated embedding. In existing `src/debug/Makefile.am`, under `C_DEBUG_LUA`,
add `dosboxluadir = $(datadir)/dosbox-x/lua/5.4/dosbox` and
`dist_dosboxlua_DATA = lua/dosbox/managed_protocol.lua lua/dosbox/managed_session.lua`.
The dist data rule distributes these authored sources and installs their basenames
under that exact directory; explicitly track both new source files. Root Makefile
already recurses into `src`. Verify `make dist` includes both sources; no second
root data rule or generated copy is needed. Confirm neither path is ignored.

The worker setup calls `require('dosbox.managed_session')`; that module and the
external host's `managed_wire.lua` both require the same pure
`dosbox.managed_protocol` for record names/version/schema validation. The protocol
module has no native binding dependency. Worker dispatcher alone owns binding
dispatch; host serializer owns client I/O. Do not mirror protocol constants/schema
or import the worker dispatcher into the external host.

The exact package launcher composes explicit paths separately for both Lua states:
child `LUA_PATH` starts with `<datadir>/dosbox-x/lua/5.4/?.lua`, followed by the
pinned dependency tree's `?.lua` and `?/init.lua`; external host additionally gets
its exact installed package Lua directory. `LUA_CPATH` names only the selected
ABI-matched module directories (plus explicit test modules in tests). No ambient
`;;`, cwd search, versioned environment override, source/install probing or fallback.
Paths are supplied by the chosen build/install recipe, not runtime discovery.
For uninstalled source tests, the first pattern is explicitly
`<absolute-source-root>/src/debug/lua/?.lua`; out-of-tree build tests use that same
authored source root plus their explicit binary/module build paths, not build-cwd
guesses. Installed tests use only the staged installed prefix and must work with
source/build trees unavailable. Include module identity/resolution, dist contents
and installed/source/out-of-tree worker+host checks in the S0-D/G gates.

**Affected clients/recipes in the same commit (current tracked inventory):**

| Owner | S0-D adoption obligation |
|---|---|
| `tests/machine-service/managed-session.py` | Direct FD3 bootstrap, old record/response grammar and module paths → v1, including allocation producer/checks |
| `tests/machine-service/session-worker.cpp` | Hand-authored old execute records/Ready expectations and allocation environment → v1; explicit new module path in invocation recipe |
| `tests/machine-service/managed-session-mcp.py` | Split unchanged dependency default-session checks from DOSBox custom-host v1 tests; no old manager decoder claim for DOSBox |
| `tests/machine-service/managed-session-saivage.mjs` | Replace custom-host old-wire assumptions with explicit v1 test-client/launcher handling; preserve separate ordinary dependency consumer checks and historical scope |
| `tests/machine-service/managed-callback-independent.mjs` | Remove textual `contextScript` slicing/data-URL import; use an explicit test-only exported scenario entry in the updated Saivage fixture accepting the constant callback test chunk, preserving independent callback-denial assertions |
| `docs/managed-lua-session.md` | Replace custom command/serve/FD3 recipes, new module install/path contract and source/build/install test invocations together |
| `docs/managed-saivage-qualification.md` | Mark retained old binary/client recipe and results as historical old-host qualification, with exact historical source pin; not evidence or a runnable current v1 recipe. Point current tests to the new v1 instructions; claim no new Saivage qualification without its separate evidence |

Also review `tests/machine-service/check-worker.sh` invocation paths and
`options.cpp` for affected setup/CLI coverage (not old-wire clients); preserve FILE/
INLINE semantics. Refresh this inventory with tracked searches for session CLI,
bootstrap environment, old record fields, module paths, wrapper/source-text imports
and doc recipes before cutover; add any newly evidenced consumer to this same unit.
The callback fixture already guards missing patch markers; its current problem is
coupling to the replaced client's text, not silent acceptance of missing markers.
Replace those DOSBox
custom-session assumptions with an explicit thin v1 test client/launcher; keep
ordinary dependency sessions on their unchanged protocol. Any new test-only helper
is marked as such and cannot become an integration Python runtime. No manager
decoder forwarding, legacy handshake probe or second serving loop. Installed
Saivage/MCP consumers are not silently upgraded or claimed qualified by these tests.

**Dependencies/requirements:** S0-C; GI-LIFE-02–04, GI-OP-01–07,
GI-TERM-01–04, GI-OBS-01, GI-PKG-02, GI-TRUST-01, GI-FAIL-01/02.

**Tests/evidence:** A0/A0b/A1 on actual Agent-off managed COM and one compatible
fresh-restored root. Add focused **new** `tests/machine-service/managed-protocol.lua`
and native/worker cases: exit between exchanges; Active wait then exit; idle and
partial-frame wake; both release/departure winners; no-operation departure; new
Execute/read/Pause refusal before evaluation; terminal observed twice; mismatched
release consumes nothing; synchronous helper completes its existing reply; release
flush failure after consumption sends no subsequent record. Invalid terminal enum
fails, unknown raw reason remains unclassified. Confirm finalizers lack guest
authority and top-level join cannot need a retired native seam. Real transport
truncation/generation mismatch must poison even inspection exchanges.
Test launcher-produced allocation through bootstrap → Ready generation zero →
subsequent exact exchanges; malformed/missing environment fails before readiness,
wrong allocation/session poisons without another request. Independent fresh hosts
may each use allocation `1`, but no old FD/session can address their new child.
Verify `ready.event_service_timeout_ms` equals the S0-B native constant, independently
of Save timeout and transport limits. Run the updated independent callback fixture
through its explicit scenario entry; preserve its actual effect-denial assertions.

**Docs:** update `docs/managed-lua-session.md` in this commit, replacing its old
serve/manager recipe and “no next chunk” rule; document v1 identity, limits,
terminal-only window, allocation producer, event deadline, exact module installation/
resolution, caller adoption and test commands. Update
`docs/managed-saivage-qualification.md` to preserve historical receipts with pinned
old reproduction instructions explicitly separated from current v1 recipes.
Preserve dependency pin,
tree/archive identity and notices. Add applicable install metadata/notices now.
**Non-goals:** dependency modification, reconnect, delivery timeout/journal,
automatic recovery, old/new union negotiation, fabricated exit causes.

### S0-E — External Lua host, protobuf/socket and schema foundation

**Objective/delta:** build the small external single-child client and one poll
loop; qualify protocol plumbing before broad presentation. One FD3 exchange and
one ordinary host-to-Ghidra request await replies independently. Reverse invocation
replies remain FIFO; Interrupt may overtake unsent refresh demand, not earlier
reverse replies or dispatched effects. Bounded output/work per turn must leave
lifecycle/terminal delivery serviceable when Ghidra is backpressured.

**Owners/files (all new):** `contrib/ghidra/lua/host.lua`, `managed_wire.lua`,
`trace_wire.lua`, `actions.lua`, `projection.lua`, `contrib/ghidra/schema.xml`,
`contrib/ghidra/dependencies.lock`, `contrib/ghidra/build.sh`,
`contrib/ghidra/tests/wire.lua`, and descriptor/package metadata under that package.
These are a small file grouping, not Client/Trace/Registry class layers. Schema
and actions belong to Lua/XML, not a native codec or Java debugger backend.
`host.lua` owns the single-allocation bootstrap value specified in S0-D; it must
overwrite any inherited allocation environment rather than trust ambient input.
`managed_wire.lua` imports the installed pure DOSBox protocol module specified in
S0-D; product host and worker use the same authored record contract.

**Build/dependency contract:** pin Lua **5.4** ABI and select/record the exact
qualified patch release, upstream archive checksum and compiler/platform in the
lock before reproducible build acceptance (the authorities do not prescribe a
patch release). Pin lua-protobuf `ee58d1a1b6df86e911f1d44194ac68247b97a93b`,
luaposix 36.3 `e6b94b37c19c4bd19a7dc475a4f4cb56f61f5da9`, and unchanged
lua-repl-mcp `0b8d271911c17ce762246afa86b74ab20e999343`. Pin Ghidra source
`8ca9077040cc43cd790babcfccf19475622a3e9a` and its exact
`Ghidra/Debug/Debugger-rmi-trace/src/main/proto/trace-rmi.proto` hash. Produce the
descriptor with that pinned Lua `protoc.lua` or an explicitly pinned native protoc;
record generator/options/output hash, ship no generated Python client. Record XML
schema and closed action-table identity separately; no hand-written protobuf codec.

Use exact 12.2 negotiation, 4-byte big-endian length, payload 1..65536; preserve
partial I/O offsets. `pb.option('int64_as_string')` plus checked canonical decimal
↔ `#` conversion preserves signed Snap/Span and unsigned Addr. Verify oneof,
optional presence, semantic enums, complete expected reply types, register
big-endian binary widths and range `extend=length-1`. Root schema belongs to
CreateRootObject, not CreateTrace. Host chooses oid 1 before CreateTrace; empty
ReplyCreateTrace doesn't assign it. Every request/reverse invocation uses the exact
open oid; close retires it, no reuse/probe after ambiguous create.

**Dependencies/requirements:** standalone wire/codec can start after approval;
actual FD3 joins S0-D and actual child launch joins S0-F. GI-LIFE-02,
GI-PKG-01/02/03, GI-TRUST-01/02, GI-FAIL-01/02, GI-UI-01/02 (declarations only).

**Tests/evidence:** exact integer endpoints, signed span, register widths, optional
fields, XML parsing and action signatures; fragmentation, oversize before allocation,
wrong reply/oid, ReplyError, FIFO interleaving and bounded backpressure. Then actual
Ghidra 12.2 negotiation/root/transaction/snapshot/register/reverse method round trip
in S0-G; synthetic protocol tests alone cannot close interoperability. No installed
package import/launch/build path may require Python.

**Docs:** **new** `contrib/ghidra/README.md`, `THIRD-PARTY-NOTICES.md` and lock/build
instructions; preserve exact dependency license texts (DOSBox GPL-derived, Ghidra
Apache-2.0, lua-protobuf MIT and actual Lua/luaposix/lua-repl-mcp notices). Record
distribution obligations/source availability; process separation is no licensing
conclusion. **Non-goals:** multi-session manager, LuaSocket alongside a second POSIX
poll layer, protocol passthrough, Python port, complete R1 model acceptance.

### S0-F — Linux process/FD ownership and independent disposal

**Objective/delta:** make external host child creation and containment real before
using it as a product carrier. Native binding is only pre-exec OS lifecycle setup;
Lua owns process policy/reaping, not debugger semantics in C.

**Owners/files:** **new** `contrib/ghidra/native/child_lifecycle.c`, focused
`contrib/ghidra/tests/lifecycle.lua` plus a small **new** native lifecycle fixture;
S0-E `host.lua`, `managed_wire.lua`, `build.sh`, lock and notices. Binding captures
expected parent before fork and arms `PR_SET_PDEATHSIG(SIGKILL)`/compares exact
parent as the first child setup operations. Fail `_exit` before exec on mismatch
or error, including non-init subreaper adoption; then setsid, FD setup, private
CLOEXEC setup-pipe arm report, ordinary nonprivileged exec. Native binding may
own the fork/setup critical sequence to keep it indivisible; it must not become a
resident guardian, PID registry, decoder or second host event loop.

Parent FD3 is nonblocking; child endpoint remains blocking, exact dup2/close works
even when an endpoint is already 3. Close inherited TCP/control/duplicates; redirect
child stdio away from Java control streams. DOSBox marks adopted FD3 CLOEXEC.
Host receives arm confirmation before publishing native-ready. Sole creator thread
remains alive; no daemonization, replacement supervisor or credential-changing exec.
Install explicit LUA_PATH/LUA_CPATH and remove conflicting versioned overrides.
Use S0-D's exact module-path contract for host and child, not an ambient path append.
Supply the host-owned allocation generation and all seven validated bootstrap
budgets before exec; arm/setup reports do not create or change that generation.

Healthy disposal: Close once on healthy channel → 5 s cooperative wait → TERM →
2 s → KILL → 5 s exact waitpid confirmation. Poisoned FD3 skips Close entirely.
Never signal after reap. Launch control remains separate from FD3, reports arm,
native/session/worker facts and reaped child exit separately; EOF is not exit.
Java ownership/forced host kill is completed in R1-A, not inferred from this fixture.

**Dependencies/requirements:** S0-E and S0-D for installed native Close;
GI-LIFE-04, GI-FAIL-02, GI-TRUST-02, GI-PKG-01/02.

**Tests/evidence:** host death before arm, after arm before report, after exec;
subreaper race; setup failure; FD3=3 descriptor collisions; descriptor inheritance;
ignored TERM escalates to KILL while host is alive; exact reap/EOF distinctions;
no ready after failed parent check. A test-owned supervisor may observe actual
child exit independently, but the product cannot claim it from host exit alone.
Verify ordinary exec and unsupported credential-changing launch exclusion. No
unbounded-exit or descendant-process containment claim.

**Docs:** package lifecycle/installation help, exact support/kill limitations and
timeouts; notices/build instructions for this small native module. **Non-goals:**
pidfd/SCM_RIGHTS transfer, Java signalling a discovered child PID, process scans,
emergency code in a dead host, other-OS portability, arbitrary process-tree cleanup.

### S0-G — Installed artifact proof and Stage 0 value decision

**Objective/delta:** qualify the combined backend/library/host foundation and close
artifact adoption, not advertise a usable Ghidra debugger prematurely.
**Owners/files:** S0 package build/install/notices/docs; existing
`tests/machine-service/check-worker.sh`, session/native fixtures and
`tests/original-foundation/README.md`; **new** `contrib/ghidra/tests/README.md` and
small exact-input acceptance recipe. Put run artifacts outside product/runtime
trees; record build, binary, config/profile, dependency, schema and fixture hashes.

**Dependencies/requirements:** S0-A–F; GI-SCOPE-02, GI-PKG-02,
GI-LIFE-01–04, GI-OP-01–07, GI-TERM-01–04, GI-FAIL-01/02,
GI-TRUST-01/02, GI-LIMIT-01.

**Tests/evidence:** A0 same sequential FD3 condition-free Run/Observe/Pause with
Agent off; equivalent sync and migrated live callers; callback proof/refusal in
external R1 profile; A0b exact dispatcher/client/docs/notices inventory; A1 COM
and fresh-restored departure/ordering/close plus transport and lifecycle failures.
Use actual Lua host and actual Ghidra 12.2 for the minimal negotiation/schema/
reverse-call proof; this is wire interoperability, not completed GUI workflow.
Before R1-A's guard exists, this Ghidra proof registers only read-only methods;
native execution proofs use the direct typed managed test client. No unguarded
standard Ghidra effect action is enabled for the prototype.
Run focused native/host tests first, then relevant real roots. Expand regressions
only for touched dispatch/teardown/input/capture owners; no all-device sweep.

**Closure decision:** re-evaluate value and simplicity against observed results:
one core, one event, one operation, one copy, one client; no hidden evaluator or
borrowed frame held for delivery. If Active, callback or teardown proof fails,
stop R1 and repair/review the precise seam. If a concrete impossibility is shown,
escalate with evidence and scope consequences. Do not change carrier or remove
supported callback behavior. Record what is proven, still open and stage-ready.
**Docs:** managed-session installation/adoption and evidence index; package scope
says internal foundation only. **Non-goals:** R1 acceptance, restore UX, deployment,
requalifying unrelated consumers or broader archive coverage.

## 4. R1 — useful COM debugger

### R1-A — Version-matched Java guard, launcher and startup lifecycle

**Objective/delta:** ship an explicitly patched isolated 12.2 consumer plus narrow
extension. No enabled standard effect may bypass selected-live-time protection.
Launch without an imported Program (`requiresImage()==false`, no-task auto-map),
retain the directly launched Lua `Process`, provide progress/cancellation before
a trace exists and maintain one 120000 ms monotonic startup budget through initial
stopped publication/activation. Java's original deadline remains authoritative;
pass remaining duration to Lua, never compare monotonic clock origins or reset per
stage. Override `AbstractTraceRmiLaunchOffer.getTimeoutMillis()` (stock trace
wait 10000 ms) and `getConnectionTimeoutMillis()` (stock connection default ten
minutes) to the positive remaining startup budget. Zero remaining fails immediately,
never means infinite. Independently bound acceptor, negotiation, create and initial
publication/activation waits by that same remaining budget; the method overrides
alone are not proof that every asynchronous wait is bounded.

**Owners/files (new package artifacts):**
`contrib/ghidra/patches/ghidra-12.2-dosbox-selection-guard.patch`,
`contrib/ghidra/extension/build.gradle`, `extension.properties`, `Module.manifest`,
`src/main/java/dosbox/ghidra/DosboxTraceRmiLaunchOpinion.java`,
`DosboxTraceRmiLaunchOffer.java`, navigation/help plugin as needed, and matching
Java tests/help under the extension. Exact Ghidra upstream patch owners are
`Ghidra/Debug/Debugger-rmi-trace/src/main/java/ghidra/app/plugin/core/debug/service/tracermi/`
`TraceRmiTarget.java`, `RecordRemoteMethod.java`, `TraceRmiHandler.java`, plus the
trace-manager coordinate event/target API callers needed to carry the stamp.
Use `gui/tracermi/launcher/AbstractTraceRmiLaunchOffer.java`'s named timeout methods
above, including their acceptor/waitForTrace call sites, in the custom offer; refresh
that source mapping before implementation. Package source patch and test additions,
not shadow classes replacing
an arbitrary user installation.

**Guard implementation boundary:** capture a call-local stamp at the first target
API/action entry before prompt, async mapping or queueing. Include selected trace,
full snap-only schedule, live trace/snap, connection/session and applicable state;
invalidate on every coordinate change, including away-and-back. Carry, never
recapture, that stamp through async work. Recheck under serialized GUI coordinate
event ordering immediately before enqueue/write; no remote wait on GUI thread.
Lowest remote-method effect dispatch refuses missing stamps. Scope to exact DOSBox
schema/protocol identity, refuse unknown methods, leave other targets unchanged.

**Invocation-path checklist (all mandatory):** toolbar and shortcuts; context-menu
ActionEntry; logical/static and dynamic breakpoint create/delete/toggle service
paths; memory/register edit target APIs; kill/Terminate helpers; direct built-in
effect calls bypassing ActionEntry. Unsupported toggle/edit/console/Detach paths
refuse even though not registered. Read-only refresh/activate stays available;
target Terminate is guarded, launcher Dispose independent, deliberate connected
trace Close disposes while Save/history browsing does not. Package launcher refuses
missing/mismatched guard identity; there is no unguarded stock fallback.

Java `ProcessBuilder` uses argv, drains host stdout control and stderr off GUI
thread. Validate HOST/PORT against combined Ghidra acceptor address, loopback by
default; connect/negotiate precedes child startup. Progress distinguishes host,
TCP negotiation, child creation/arm, FD3/bootstrap, native-ready, create/publication.
Cancel/timeout preserves known stage facts, then separate cleanup: host disposal
request up to 15 s, retained host `destroyForcibly` if hung, up to 5 s for host exit.
Child disposal remains unconfirmed without exact child-exit evidence even when
arm and host exit are known. Never ask poisoned FD3 to Close/Observe.

**Dependencies/requirements:** S0-E/F, S0-G before combined acceptance;
GI-TIME-01, GI-LIFE-03/04, GI-UI-02, GI-PKG-01, GI-TRUST-02,
GI-FAIL-01/02, GI-FLOW-01/02.

**Tests/evidence:** focused patched Java target/remote-method tests, then A4 actual
GUI toolbar/keybinding/context/breakpoint/Terminate and queued away/back selection
change with backend request observation. Demonstrate valid live dispatch isn't
retroactively cancelled. A2 launch >10 s but <120000 ms; expiry/cancel before ready
and after ready during create/publication; no stage resets or GUI blocking. A5
hung-host Java kill with independent test evidence, unconfirmed child reporting,
and actual lifecycle controls. Build pinned 12.2/JDK25 with audited patch and exact
extension identity; a source-only guard test is not release acceptance.

**Docs:** package build/install/help, explicit patched distribution choice and
identity, startup/cancel/disposal meanings, guarded path inventory. **Non-goals:**
Trace RMI wire changes, Java backend/MMU, user-install patches, auth-token registry,
nondestructive detach, retrospective cancellation of dispatched effects.

### R1-B — Native observations, COM provenance and hit identity

**Objective/delta:** deliver facts required by actual consumers: ordinary-real-mode
admission from loaded cache/mode validity; safe intervals/read bounds; exact stopped
revision; actual COM PSP/entry/image extent/load identity; breakpoint native/Lua
IDs, kind/owner/internal/temporary/count and actual hit attribution. Unsupported
stopped contexts retain available raw facts but revoke misleading address features.
Separately, detect and publish the existing native load/exit fact needed to invalidate
selected COM load authority on unsupported child EXEC, overlay or address-reuse/load
replacement, even when CPU-mode admission remains valid. R1-B owns this bounded
invalidation and its session/status projection; ordinary-real-mode status alone
must not keep a replaced load current. Use the actual load/exit owner, not current
CS, filename, a Lua counter or a new lifetime registry. Publication and at-use
admission must prevent the old load selection from authorizing subsequent mapping-
derived breakpoint placement; no automatic revalidation on child return is claimed.

**Owners/files:** `src/debug/debug.cpp` `LocalDriverImage`, native hit producer,
registration and program-entry capture; `src/debug/debug_common.cpp/.h`,
`src/dos/dos_execute.cpp` actual load/overlay/exit owner and
`src/debug/debug_lua.cpp` selected-entry receipt; `include/lua_driver_handoff.h`, worker
projection and S0-D typed schemas. Inspect/reuse `src/cpu/paging.cpp::PAGING_InspectLinear`,
`src/hardware/memory.cpp` safe handler layout and CPU loaded-cache owners; change
them only for necessary nonmutating fact projection, not a new translator.

**Dependencies/requirements:** S0-G; GI-UI-03, GI-BP-01/02, GI-OBS-01,
GI-MEM-01/02, GI-ADDR-01/02, GI-MAP-01/02, GI-OP-06.

**Tests/evidence:** native safe-read sentinel, no MMIO handler invocation, no guest
progress/fault/accessed-bit change; exact stale/running refusal; loaded-cache
ordinary-mode versus unreal/protected rejection. Below-1-MiB alias conversion
round-trip; logical offset+length >65536 refuses while linear read advances.
Two user breakpoints and internal Over/run-until distinguish actual native IDs,
not PC guesses. COM entry metadata survives far execution without deriving load
identity from new CS. Extend `breakpoint-identity.py`, `memory-observation.py`,
`descriptor-observation.py` and managed runtime fixtures only where applicable.
Add a focused native load-replacement case with otherwise supported real-mode
facts: detect the actual invalidation and expose it before a later stopped request
can use stale load authority. Inspect child EXEC, overlay and load/address reuse
paths for coverage; a startup receipt or selected-owner departure alone is not
proof, particularly because overlay returns without the ordinary PSP setup path.
If the existing native seam cannot supply this bounded fact for the R1 refusal
boundary, stop and review R1-B before its dependent consumer stage proceeds;
neither defer correctness to L-A nor silently add a lifecycle framework.

**Docs:** managed API/capability/coordinate semantics and fixture limits; package
protocol/schema notes updated with producers, including load-validity facts and
unsupported-transition refusal independent of CPU-mode validity.
**Non-goals:** useful child-return/TSR/overlay/address-reuse modelling (L-A),
watchpoint expansion,
target writes, universal A20/TLB equivalence, loader discovery, descriptor dumps,
legacy side-effectful read substitution or static-program rebasing.

### R1-C — Early real-GUI COM vertical and segmented-semantics decision

**Objective/delta:** exercise the smallest useful actual consumer path before full
adapter expansion. Lua projects real stopped COM into typed Registers, frame-zero
Stack/PC, Listing, Memory Map and Modules, activates cpu0/frame0, and supplies
native Run/Pause/Step Into/Over. Use `x86:LE:16:Real Mode`/`default`, true logical
EIP and exact-width register overlay; explicit effective SS:SP navigation. One
guest CPU thread only. Pending is not RUNNING; Pause request is not STOPPED.

**Owners/files:** S0-E `schema.xml`, `actions.lua`, `projection.lua`, `host.lua`;
R1-A extension stack navigation/help. **New** small COM assembly fixtures under
`contrib/ghidra/tests/fixtures/`, and actual GUI test/recipe sources under the
package tests (Java/Lua; existing external Python drivers may be reused as tests).
Use the complete typed method signatures from design §6.4; do not advertise
not-yet-implemented methods or register evaluator/console/target-edit fallbacks.

**Dependencies/requirements:** R1-A/B; GI-SCOPE-01, GI-FLOW-01,
GI-LIFE-03, GI-UI-01–03, GI-ADDR-01–03, GI-PKG-01/03.

**Tests/evidence:** actual Ghidra window + actual Lua host + actual DOSBox, not a
constructed trace. Check initial activation and skipped register names, native
bytes/instructions, true registers, frame-PC and stack navigation. Use nonaligned
`1234:0100`, AX=`0200`, `JMP AX` native destination `0x12540`, near-relative
normalized-boundary and far-transfer cases. Separately record dynamic references/
p-code targets versus native destinations. Manually correlate the COM module to
an imported Program after launch. Guarded effects only; second native attachment
must refuse. Small bounded snapshots precede on-demand memory.

**Proof gate:** decide whether persistent limitation notices and disabling/clearly
labelling affected evaluated-target/p-code navigation make the ordinary workflow
truthful. If not, stop expansion, design/review the narrow demonstrated consumer
remedy before shipment. A PC tracker is not a SLEIGH repair. Concrete impossibility
escalates; no carrier switch, all-mode language or universal decompiler project.

**Docs:** package walkthrough, explicit dynamic-analysis limits, test evidence and
current supported action list. **Non-goals:** full R1 acceptance, unwinding,
invented symbols/threads, treating static mappings/DS properties as pointer fixes.

### R1-D — Complete mapping, breakpoint and observation/time workflows

Keep the following as three coherent commits if each leaves the internal package
working; each includes its tests/docs and requires R1-C's consumer gate. They
jointly deliver R1-D and are not independently advertised as full R1.

**R1-D1 — COM mapping and instrumented execution.** Objective/delta: stock ComLoader
and manual Map Module/Section where needed use actual PSP/load extent/lifespan,
static `0000:0100` → runtime `PSP:0100`, never filename/current-CS matching.
Create/delete native execute locations from selected mapping; expire location
on known deletion and preserve actual user/internal hit identity in Breakpoints.
Consume R1-B's invalidation fact to retire the current trace module/static mapping
lifespan or explicitly project it unavailable, preserving historical observations
and static Program annotations. Disable and refuse address-sensitive breakpoint
placement through that stale mapping, including requests queued before invalidation;
neither a live GUI time selection nor supported CPU mode restores its load authority.
An unavailable projection must actually block use of the stale mapping by the
standard breakpoint path, not merely add a descriptive Model property. If that
requires narrow Java consumer handling, include it with this slice and its tests.
Owners: Lua actions/projection/schema, R1-B native registration only if a revealed
consumer gap needs it, extension mapping consumer/help; new COM two-breakpoint/call/write
fixtures. Requirements: GI-FLOW-01/02, GI-BP-01/02, GI-MAP-01/02, GI-UI-01–03.
Tests/evidence: A2 two distinct user hits, internal Over, aliases, deletion,
static labels/comments reused across two separate launches, synthetic static PSP
distinguished from captured bytes. Add one focused actual Ghidra/Lua/DOSBox A2
consumer case: map a COM, execute an unsupported load replacement (for example an
overlay into its mapped range), reach the next controlled stop with ordinary-real-
mode admission still true, and verify the old module/mapping is retired or visibly
unavailable. Attempt a static-listing breakpoint through the old mapping, including
a queued stale placement: no native registration may be created from that stale
authority. Verify history/static annotations remain, and returning to a live time
does not revive the invalid mapping. This proves the R1 refusal boundary, not useful
overlay or child-return support. Pair it with R1-B's native path-coverage checks;
GUI decoration alone cannot pass. Docs: mapping/breakpoint recipe, unsupported-load
transition behavior, refusal/relaunch limits and exact consumer evidence.
Non-goals: automatic discovery, permanent rebase, toggle via delete/recreate,
MZ sections when COM module alone suffices, implicit copying to static analysis.
Useful child-return, TSR, overlay and address-reuse modelling remains in L-A;
R1 only invalidates unsupported load authority, without a lifecycle framework.

**R1-D2 — Safe bounded memory, snapshot freshness and observation batching.**
Objective/delta: every batch is bound to exact session/revision/current live snap;
all memory/register/object writes name explicit snap/span independent of historical
cursor. Mark advertised RAM unknown at activation/new stop, then publish only fresh
success as known and actual failures as error. Split 4096-byte GUI requests across
native maximum/safe intervals/page boundaries, all pieces or explicit holes;
failed subreads publish no bytes. Owners: `projection.lua`, `host.lua`,
`managed_wire.lua`, `trace_wire.lua` and package fixtures. Requirements:
GI-FLOW-02, GI-OBS-01, GI-MEM-01–03, GI-ADDR-01/02, GI-PKG-03, GI-FAIL-01/02.
Tests/evidence: A3 Listing/time page captured then changed by guest then unrecaptured
new stop remains unknown; historical refresh updates only matching live stop;
4 KiB split with smaller advertised native cap, holes, failed whole range, logical
wrap refusal versus linear read, no unconditional RAM/table dump, representative
no-side-effect sentinel. Docs: memory states, range meaning, incomplete observations.
Non-goals: bytewise discovery, side-effectful fallback, full RAM hashes per stop,
historical memory reconstruction or claiming publication atomic with the guest.

**R1-D3 — Scheduling, closed action set and failure/lifecycle integration.**
Objective/delta: coalesce stopped refresh demand; after one short in-flight exchange
prioritize exact Interrupt without reordering FIFO reverse replies. Complete obsolete
refresh unavailable rather than queue guest reads behind Run. Copy terminal facts
before release independently of Ghidra backpressure. Open short trace transactions
only after native exchange and never across effects; report incomplete publication
without replay. Owners: Lua host/actions/wire/projection and R1-A lifecycle hooks;
focused fault fixtures under package tests. Requirements: GI-OP-04/06,
GI-LIFE-01/04, GI-UI-02/03, GI-TIME-01, GI-FAIL-01/02, GI-TRUST-01/02,
GI-PKG-03. Tests/evidence: A5 refresh burst→Interrupt ordering, backpressured TCP
with departed delivery/disposal, truncated effect reply sends no next generation
including Close, complete inspection refusal permits new reads, complete Active
Wait permits polling, publication failure preserves known effect. Test guarded
Terminate, independent Dispose, connected Close and inert Save/browse effects.
Docs: scheduling/failure decision table and control meanings. Non-goals: retry,
reconnect, exactly-once guarantees, arbitrary method forwarding, a trace transaction
around execution or lossless delivery promises.

### R1-E — Installable COM release and consumer-path acceptance

**Objective/delta:** release the combined qualified Linux/SDL2, heavy-debug/normal
core where required, managed-Lua package, patched Ghidra 12.2 and JDK25 composition.
**Owners/files:** package `build.sh`, dependency lock, descriptor/schema, Java
extension/patch build wiring, install/help/notices, package test index; root
distribution metadata only where needed. **New** package acceptance report template
records actual source/patch/module/build/profile/fixture identities and limitations.
Do not copy downloaded research trees into DOSBox source or opportunistically edit
the user's Ghidra. Install into a dedicated prefix, with reproducible patch apply
and guard identity check; inspect actual licenses/source-distribution obligations.

**Dependencies/requirements:** R1-D1–D3; all R1 rows in §7, particularly
GI-SCOPE-01/02, GI-PKG-01/03, GI-TRUST-02, GI-LIMIT-01.

**Tests/evidence:** clean install with no Python integration dependency, Agent/RPC,
MCP or provider; actual A2–A5 using the installed artifacts. Include all standard
windows, no-Program launch then manual mapping, two launches/static reuse, historical
effect routes, segmented limitations, memory freshness, no silent writes, startup
timeouts/cancellation, host-death and ambiguous-channel disposal. Preserve failed
attempts and their interpretation. Run native-focused tests first, host protocol
tests next, real GUI next, lifecycle faults, then broader affected DOSBox regression:
interactive/bridge/RPC and live Lua sequentially, FILE/INLINE scripts, input/capture,
program-exit/restored-root/save checks only where touched. Reuse unchanged evidence;
don't manufacture confidence with repeated broad hashing or every device profile.

**Release gate:** independently assess exact combined evidence, usability, KISS and
scope. Explicitly list unqualified dynamic semantics; if supported normal workflow
remains misleading, no R1 release. Deliver actual commands/results, not screenshots
alone or schema existence. Deployment/adoption by PM or other installations is separate.
**Docs:** install/uninstall, launcher, actions/limits, trace data sensitivity,
failure/disposal recipes, native docs and test index. **Non-goals:** MZ/restore UI,
HMA/unreal/V86/protected support, target edits, general attach/detach, Save action.

## 5. Independent later releases

### R2a-A — MZ module/sections and reusable mapping

**Objective/delta:** project actual MZ load segment, PSP, entry, image extent,
relocation relation and factual load identity; stock MzLoader's imported bases
drive Section/Module mappings. Distinguish legitimate relocation-word changes
from wrong image and self-modified runtime bytes.
**Owners/files:** `src/dos/dos_execute.cpp`, native load projection in
`src/debug/debug.cpp`, handoff/worker/schema producer-consumer changes as needed;
Lua schema/projection/actions and extension mapping help; **new** MZ fixture under
`contrib/ghidra/tests/fixtures/`. Add only actual Section interfaces/attributes.
**Dependencies/requirements:** R1-E, not R2b; GI-MAP-02/03, GI-UI-01,
GI-SCOPE-01/02, GI-OBS-01, GI-MEM-03, GI-LIMIT-01.
**Tests/evidence:** A6 one relocatable MZ with separate code/data/stack and near/far
calls at two load bases using the same static Program/annotations; relocation
words truthful, one code modification re-disassembles dynamically without static
overwrite. Native load fixture first, then actual GUI mapping, focused R1 regression.
**Docs:** MZ install/profile/mapping/relocation limitations and A6 receipt.
**Non-goals:** general DOS loader tracing, arbitrary overlays/extenders, relocation
fixups via range mapping, runtime-DS decompiler correction, mandatory R2b adoption.
Land native projection with its Lua consumer/fixture as one coherent commit; a
separate mapping/help commit may follow if the initial package advertises no MZ
mapping until complete. Reassess value/simplicity before this release.

### R2b-A — Fresh restore into a new process and new trace

**Objective/delta:** separate launch choice for compatible archive/profile using
existing fresh-initialized restore; no program replay. New process/session/trace
must expose saved PC/registers/selected bytes stopped before explicit continuation.
Old handles cannot address it; optional user breakpoint intent becomes new native
registrations. No interpreter state or fabricated prior debugger events.
**Owners/files:** R1 Java launch offer/help, Lua host/actions/projection, existing
`include/lua_driver_options.h`, `src/gui/sdlmain.cpp` restore/program selection only
if UI plumbing reveals a necessary change; current fresh-root/native restore owners
remain authoritative. **New** package GUI restore recipe/fixture; reuse
`tests/original-foundation/fresh-startup-lua.py`, `fresh-startup-no-replay.py`,
`fresh-startup-terminal.py` and `restored-root.py` for affected native regressions.
**Dependencies/requirements:** R1-E, not R2a; GI-FLOW-03, GI-REST-01/02,
GI-LIFE-03/04, GI-FAIL-01/02, GI-SCOPE-02, GI-LIMIT-01.
**Tests/evidence:** A7 real archive selection → new stopped process/trace → saved
observations with no instruction/timer progress → explicit Run reaches known result.
Reject incompatible archive/profile with truthful disposal; stale object/session
requests refuse. Repeat startup/cancel/lifecycle checks at this launch route,
not a full archive/device matrix. Stage 0 fresh-root proof is reused native evidence,
not substitute GUI evidence.
**Docs:** archive-versus-trace-versus-static Program distinction, supported profile,
new-lifetime UX and independent release/evidence notes.
**Non-goals:** in-place restore, reverse execution, interpreter/connection restoration,
automatic breakpoint replay, guest launch replay, in-Ghidra Save. Keep this as one
launch/host/test/help changeset unless a separately necessary native fix first lands
coherently. Reassess simplicity and value before release.

### L-A — Independent DOS lifetime increments

**Objective/delta:** qualify child EXEC/return, then TSR residency, then overlay/
address reuse as separate bounded packages when requested; each projects actual
load lifetime, expires replaced mappings and preserves evidenced residency.
**Owners/files:** existing DOS load/exit owners in `src/dos/dos_execute.cpp` and
associated native debugger metadata, Lua module/section projection and mapping
help; relevant existing `dos-overlay.py`, `fresh-child.asm`, `fresh-int27.asm`
fixtures and **new** GUI lifespan cases as needed.
**Dependencies/requirements:** relevant released mapping baseline; GI-ADV-01,
GI-MAP-02, GI-SCOPE-02, GI-LIMIT-01. No dependence on protected-mode support.
**Tests/evidence:** A8 for the chosen transition, same address/new load identity,
child return not whole guest death, exact mapping expiry and truthful resident
range, through actual GUI plus native producer tests; run affected exit tests.
**Docs:** the one newly qualified lifetime, observable unsupported transitions and
mapping limits. **Non-goals:** universal loader coverage, process/thread fiction,
overlay discovery framework or completing every lifetime before any ships.
Reassess each increment; require a fresh bounded plan if actual owner changes
exceed these seams. This is not authorization to implement all later lifetimes.

### L-B — Protected/mixed-mode representation proof, then bounded profiles

**Objective/delta:** first establish actual consumer decoding/navigation with native
effective translations before selecting representation. Compare fixed flat-host/
guest-platform approach versus mode-specific views using a small fixture; do not
implement both as production fallbacks. Then plan one approved protected16,
protected32 or mixed-mode profile at a time, independently of L-A.
**Owners/files:** existing `src/cpu/cpu.cpp`, `src/cpu/paging.cpp`,
`src/hardware/memory.cpp` loaded-cache/MMU owners and debugger projection;
Lua schema/projection and narrow Java platform/navigation consumer only as the
experiment proves necessary. **New** profile fixture/receipt under package tests.
**Dependencies/requirements:** R1-E, representation decision before production
profile code; GI-ADV-02, GI-ADDR-01/02/03, GI-MEM-01, GI-SCOPE-02, GI-LIMIT-01.
**Tests/evidence:** A8 discriminating current-descriptor versus loaded cache,
equal selectors/unequal caches, default sizes/far pointer/invalid selector,
nonidentity paging and table/TLB divergence as relevant. Native effective query
must not fault, fill caches or change accessed/dirty state; distinguish lookup,
walk and backing bytes. Actual GUI decoding/mapping is mandatory. HMA additionally
requires A20 proof and real reproduction/remedy of 12.2 pre-dispatch final-block
quantization (`FFFF:FFF0`), not merely host-side read splitting.
**Docs:** profile-specific coordinates, supported native query meaning, fixed trace
language and consumer limitations. **Non-goals:** Lua/Java MMU, new CPU language or
Ghidra fork by implication, per-selector registry, all extenders or universal mode
coverage. Representation failure stops/revises the profile; obtain a scope decision
for any stronger architectural change. Reassess simplicity before each release.

## 6. Proof gates, evidence and bounded validation

| Gate | Required proof / stop condition |
|---|---|
| G0 source/adoption | Fresh owners/callers and coherent commit boundaries; drift or unknown supported caller behavior blocks its cutover |
| G1 native core | S0-A–C focused execution/event/live regression; no Pending forced activation, stale Pause, stranded event or hidden old engine |
| G2 actual carrier | A0/A0b/A1 plus Lua wire/lifecycle foundation; absence of same-FD3 Active control, terminal-only delivery, callback preservation or strict artifact adoption stops R1 |
| G3 actual consumer | R1-C real GUI nonaligned-CS/typed views and early guarded actions; misleading normal workflow requires reviewed narrow remedy |
| G4 R1 release | Installed A2–A5 including R1-B/D1 unsupported-load invalidation and stale-mapping breakpoint refusal; all guard paths/startup/lifecycle, appropriate broader regressions and scoped claims |
| G5 independent extension | A6, A7 or chosen A8; fresh design-value/simplicity review before each release |

At G0, re-read the specification's mandatory focused subcases for every mapped
requirement in the proposed changeset and assign each applicable case to its proof
recipe. At G4, re-read those subcases for every R1 requirement and check the exact
combined evidence against them. The A-number index in §7 does not replace these
subcases; later release gates apply the same check to their newly delivered scope.

R1-B has an explicit stop-and-review dependency before R1-C/D1: establish the
bounded native unsupported-load invalidation fact. Missing native coverage cannot
be excused by supported CPU mode, an unchanged PSP/CS or deferral of useful lifetime
modelling to L-A. R1-D1's actual consumer refusal case must pass before G4 release.

Validation order is risk-based: focused native ownership tests → Lua host/protocol
tests → actual Ghidra GUI/Trace RMI consumer → process-death/timeout/failure fixtures
→ broader DOSBox regressions justified by touched owners. Some lifecycle primitives
must pass early before creating real children; they are repeated at the Java-owned
chain only because that adds a new consumer/lifetime. No fixed coverage percentage,
full historical suite per edit, exhaustive interleaving or device sweep is required.

Use existing test invocation conventions: `tests/machine-service/check-worker.sh`
and focused C++ fixtures, actual `managed-session.py`/`lua-production-runtime.py`
with task-owned `SAIVAGE_CARD_WORK_ROOT`, `DOSBOX_TEST_BINARY` and installed module
paths, and new Lua/Java package tests. Refresh exact build/test commands from
scripts before running; Python here is test-only. Each meaningful receipt records
source/build/package/dependency/guard/fixture/profile identity, command, exit/result,
expected versus observed behavior, failures and limits. Screenshots supplement
wire/native evidence, not replace it. No runtime success is claimed in this plan.

No failed proof authorizes Python/RPC fallback, weakened tests, speculative generic
infrastructure or removal of existing callbacks. Prefer fixing/removing the precise
introduced mechanism; if the required contract is concretely impossible, escalate
the evidence and options for an explicit decision. Stage 0 closure and every release
reassess whether the result remains worthwhile, minimal and within authorized scope.

Uncertainty is preserved at every gate: complete known refusal or inspection failure
allows a fresh deliberate healthy-channel request; complete Active/Pending Wait
allows exact observation; malformed/incomplete/mismatched/timed-out exchange poisons
the channel, including reads. No next record, protocol Close, retry, reconnect or
probe follows. Use independent disposal, distinguish requested/confirmed/unconfirmed
exit, and retain already-known effects. Trace publication failure never rolls back
native effects or authorizes their replay. Native terminal delivery has no automatic
timeout; startup and explicit disposal budgets are different policies.

## 7. All 48 requirements mapped to delivery owners

Acceptance IDs refer to the functional specification, including its mandatory
focused subcases. A later-stage requirement does not gate R1. Shared scope/claim
review is conducted with each release, not an extra synthetic runtime scenario.

| GI requirement | Work-package coverage | Evidence gate |
|---|---|---|
| GI-SCOPE-01 | R1-C/D/E; R2a-A/R2b-A | A2, A6/A7 extension |
| GI-SCOPE-02 | S0-G, R1-E, R2a-A, R2b-A, L-A/B | scoped independent releases |
| GI-FLOW-01 | R1-A/C/D1/E | A2 |
| GI-FLOW-02 | R1-A/D1/D2/E | A2/A3/A4 |
| GI-FLOW-03 | R2b-A | A7 |
| GI-LIFE-01 | S0-A/C/G, R1-C/D3/E | A0/A5 |
| GI-LIFE-02 | S0-A–G | A0/A0b |
| GI-LIFE-03 | S0-C/D, R1-A/C | A0/A2 |
| GI-LIFE-04 | S0-D/F/G, R1-A/D3/E, R2b-A | A1/A2/A4/A5 |
| GI-OP-01 | S0-C/D/G | A0 |
| GI-OP-02 | S0-C/D/G | A0/A1 |
| GI-OP-03 | S0-A/C/D/G | A0 |
| GI-OP-04 | S0-A/C/G, R1-D3 | A0/A5 |
| GI-OP-05 | S0-C/D/G | A0/A1 |
| GI-OP-06 | S0-C/D, R1-B/D3 | A0/A2 |
| GI-OP-07 | S0-B/C/D/G | A0 |
| GI-TERM-01 | S0-B/D/G | A1 |
| GI-TERM-02 | S0-D/G | A1 |
| GI-TERM-03 | S0-D/G | A1 |
| GI-TERM-04 | S0-D/G | A1 |
| GI-UI-01 | S0-E declarations, R1-C/D1/E, R2a-A | A2; A6 sections |
| GI-UI-02 | R1-A/C/D1/D3/E | A2/A4 |
| GI-UI-03 | R1-B/C/D1/D3, including native load validity and stale-mapping action refusal | A2 unsupported-load consumer case; A5 |
| GI-BP-01 | R1-B/D1 | A2 |
| GI-BP-02 | R1-B/D1 | A2 |
| GI-OBS-01 | S0-B/C/D, R1-B/D2 | A0/A3 |
| GI-TIME-01 | R1-A/D3/E | A4 |
| GI-MEM-01 | R1-B/D2 | A3 |
| GI-MEM-02 | R1-B/D2 | A3 |
| GI-MEM-03 | R1-D2, R2a-A | A3/A6 |
| GI-ADDR-01 | R1-B/C/D2 | A2/A3 |
| GI-ADDR-02 | R1-B/C/D2 | A2/A3 |
| GI-ADDR-03 | R1-C/E, L-B profile extension | A2; A8 later |
| GI-MAP-01 | R1-B/C/D1 | A2 |
| GI-MAP-02 | R1-B/D1 unsupported-load invalidation/module-mapping retirement; R2a-A, L-A useful later modelling | A2 replacement with supported CPU mode; A6/A8 |
| GI-MAP-03 | R2a-A | A6 |
| GI-ADV-01 | L-A | A8 chosen lifetime |
| GI-ADV-02 | L-B | A8 chosen profile |
| GI-REST-01 | S0-G bounded native precursor, R2b-A UX | A1/A7 |
| GI-REST-02 | R2b-A | A7 |
| GI-FAIL-01 | S0-B/C/D/E/G, R1-A/D2/D3, R2b-A | A1/A5/A7 |
| GI-FAIL-02 | S0-D/E/F/G, R1-A/D2/D3, R2b-A | A1/A5/A7 |
| GI-PKG-01 | S0-E/F, R1-A/E | A2 installed composition |
| GI-PKG-02 | S0-D/E/F/G | A0b |
| GI-TRUST-01 | S0-D/E/G, R1-D3/E | A0b/A5 |
| GI-TRUST-02 | S0-E/F/G, R1-A/D3/E | A5 |
| GI-PKG-03 | S0-A/E, R1-C/D2/D3/E | A3/A5 |
| GI-LIMIT-01 | every package non-goals; S0-G/R1-E/later release gates | scope/claim review |

## 8. Review readiness and documentation adoption

Self-review at the frozen HEAD: the existing handoff couples Executing/Event and
terminal/closing; `sessionRead` blocks on recv and `managedSession` still calls
dependency serve; `LocalDriverFrontier` has the condition-free early return and
held preparing/dispatch sequence; `Normal_Loop` Pending handling is Agent-gated.
These remain required changes, not capabilities accidentally assumed delivered.
The design's Ghidra patch/extension boundary and Linux parent-death policy are
preserved. New source paths are proposals, not existing APIs. The exact Lua5.4
patch/build hash remains a package selection/qualification obligation, not an
invented approved pin. All 48 IDs have delivery and evidence owners above.

Implementation documentation must update current managed-session and affected
Lua/live contracts as their code lands; package install/help/notices and test
indices accompany each release. Keep approved analysis/spec/design as authority,
not rewrite them to imply test results. A necessary contract change is explicitly
reviewed against those authorities, not smuggled into implementation notes.

This R3 plan is **approved for staged implementation**, based on Sol's independent
R3 approval and the author's source-grounded review and final clean-room read,
under the owner's Sol-only closure decision. No GLM or DeepSeek R3 approval is
claimed; NVIDIA is excluded. No blocking contradiction remains in this plan review.
All source-freshness, coherent-cutover, actual consumer, lifecycle and release proof
gates above remain mandatory; plan approval is not evidence that any gate has passed.
No implementation has begun. No code, tests, build files, runtime, deployment or
approved authority documents are changed by this plan closure.
