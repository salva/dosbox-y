# Ghidra integration — technical design

Date: 2026-11-10. Revision: 3 (Lua-only revision 2). **Status: independently approved (Sol, GLM 5.3 and DeepSeek).**
This is a design, not implemented behavior or runtime qualification. It authorizes
no implementation, deployment, runtime action, or consumer migration.

## 1. Authority, inputs and decisions

Current authorities are the owner's Lua-only correction and the coherently revised
analysis/specification alongside this design. Earlier analysis `d8710d20e` and
specification `d2ac6b736` preserve reviewed history; their Python-specific architecture
and this design's previous Python placement are superseded, not still approved.
All 48 GI identifiers and non-Python behavior/races/stages are retained. The Lua-only
carrier, packaging and trust mechanisms are independently approved, not implemented
or runtime-qualified. The sibling documents are
[analysis](GHIDRA-INTEGRATION-ANALYSIS.md) and
[specification](GHIDRA-INTEGRATION-FUNCTIONAL-SPEC.md).

Source checked for this design:

| Input | Identity and decisive seams |
|---|---|
| DOSBox-Y | Reviewed HEAD `d2ac6b736` in the `original-savestate-restart` repository/worktree; historical document pins above refer to that repository, and approved documents follow baseline `bd053ee4e8a5b258191b3f90212244f7bd4066e7`. `include/native_debug_execution.h`: attachment, context, enqueue/activate/settle/release. `include/lua_driver_handoff.h`: single exchange currently coupled to executing operation/event. |
| Managed native/worker | `src/debug/debug.cpp`: `LocalDriverObserveHit`, `LocalDriverFrontier`, `LocalDriverRetire`, `Debugger_LivePhaseCompleted`; `src/debug/lua_driver_worker.cpp`: `invoke`, `module`, `sessionRead`, `managedSession`, `runWorker`. |
| Existing stop-only seam | `src/agent/bridge/agent_bridge.cpp:EmulationThreadQueue`: Agent-gated, refuses Pending wholesale, permits stop-only work during Active. It is prior implementation to factor, not an available managed-Lua control service. |
| Ghidra | Local researched tree verified at `8ca9077040cc43cd790babcfccf19475622a3e9a`, Ghidra 12.2. `TraceRmiTarget`, `RecordRemoteMethod`, `TraceRmiHandler`, `AbstractTraceRmiLaunchOffer`, and approved analysis's pinned loader/language/consumer inventory. |
| Lua libraries | Exact Git blob of `lua_repl_mcp/session.lua` inspected at unchanged pin `0b8d271911c17ce762246afa86b74ab20e999343`; provenance/tree/archive recorded in `docs/managed-lua-session.md`. |
| Project guidance | Parent DOSBox-Y `AGENTS.md` and `TODO.md`; no nested AGENTS file at repository root. Preserve unrelated supported frontends/input/capture and dirty work. |

Hard decisions:

1. External Lua integration host → typed managed Lua → existing exclusive native owner. No Python runtime component, Agent
   controller/RPC backend, fallback, second evaluator, or second CPU loop.
2. One async operation core; synchronous helpers and affected `dbg.live` execution
   bindings become callers of it. Existing callback behavior migrates, not vanishes.
3. A DOSBox-owned version-1 managed protocol replaces this host's dependency
   `session.serve` invocation. Dependency bytes and ordinary dependency clients
   retain their own identities; no protocol fork or decoder forwarding.
4. Native departure transfers one terminal copy to the same session, with no
   borrowed guest frame retained for remote delivery.
5. R1 uses a **documented, isolated, version-matched Ghidra consumer patch plus
   extension**. A launcher-only extension cannot interpose all observed standard
   effect paths. Do not install a shadow class over an arbitrary Ghidra installation.
6. R1 uses real-mode language/default compiler, frame-zero PC and explicit SS:SP
   navigation. It does not promise corrected dynamic p-code/branch references.
   Actual consumer evidence must demonstrate a truthful usable restricted UI.

Names below are proposed current contract names. File subdivision, build wiring,
exact GUI labels and calibrated timeout values belong to the implementation plan;
the ownership, validation, ordering and refusal rules do not.

## 2. Components and bounded ownership

```text
Ghidra 12.2 isolated distribution / JDK 25
  DOSBox extension: launch offer, help, effective stack navigation, limitations
  narrow consumer patch: selection-guarded standard effect dispatch
   stock Java views + Trace RMI 12.2 trace/object/transaction consumers
                       ⇅ loopback Trace RMI 12.2
Lua integration host (launcher, child owner and one I/O event loop)
  Lua Trace RMI schema/actions/explicit snapshot projection
  protobuf codec + POSIX sockets/process primitives; typed managed records
                       ⇅ inherited blocking socketpair endpoint FD3
DOSBox managed-session owner (process lifetime)
  one reader/dispatcher on sole Lua worker; DOSBox schema + pinned libraries
  one short exchange slot; one departure delivery slot; local wake descriptor
                       ⇅ existing handoff, split exchange/operation lifetimes
Native debugger service (emulator thread)
  NativeDebug::Execution + exclusive attachment + exact stopped context
  shared active control service / one provisional hit / native instrumentation
                       ⇅ existing frontiers / ordinary CPU dispatch
DOS guest
```

Proposed artifact locations are `src/debug/lua/managed_session.lua` and
`managed_protocol.lua`, packaged under a DOSBox namespace; native changes stay in
the existing debugger/handoff/worker owners with a small shared native service
unit if extracting it improves clarity. External integration lives under one
`contrib/ghidra/` package with Lua host/message helpers/schema, Java extension, explicit
12.2 patch, launch metadata and help. Do not introduce a transport/plugin framework.

Native `Execution` remains emulator-thread-only. The handoff mutex protects copied
exchange/status/event/delivery data, not guest execution. A worker never dereferences
an `Execution` pointer or invokes CPU control directly. One request slot plus a
pending-control flag suffices for the one managed client; no operation registry.
The existing bounded registration collection remains the owner of user native IDs
and worker callback references, not an invented per-client broker.

The external Lua host owns one child process lifetime and one FD3 client. The Java
launcher retains the external host's actual `Process`; it can terminate a hung host.
A small Linux pre-exec lifecycle binding arms parent-death SIGKILL in the DOSBox
child and checks its expected parent (§10). Kernel enforcement survives host death;
no child handle is transferred to Java. This is process containment, not another
debugger connection. FD3 is never shared with
Ghidra, ordinary subprocesses or an MCP manager. The Lua worker owns its interpreter;
native session ownership outlives Lua calls, not vice versa.

### 2.1 Lua-only placement decision and verified dependencies

**Select A: external Lua host**, not embedded Trace RMI (B). A adds one serialized
managed hop, but keeps Ghidra I/O, trace transaction failures and codec/package
versions outside the sole embedded worker and native process. Its one event loop
can service child exit/disposal while Ghidra is backpressured, and copy/release a
departed result before publishing it. B can use local bindings without FD3 control
serialization, but still requires an external Lua process owner and lifecycle
channel; it would put bidirectional Trace RMI publication and method dispatch into
the sole worker that already owns callback/event and terminal-delivery duties.
It broadens worker lifetime/multiplexing rather than eliminating it. B is feasible
in principle, not selected or a fallback. No additional guest evaluator/controller
is introduced by A: the external Lua program executes fixed integration logic only.

The host is not a Python architecture translated into Lua. Use one `host.lua` entry
with explicit pending-I/O/child/operation fields, small `trace_wire.lua` and
`managed_wire.lua` serializers/validators, and schema/projection tables. No Client /
MethodRegistry / Trace class hierarchy, general RPC framework, thread pool or
multi-session manager. Lua coroutines may represent suspended local transactions,
but only the event loop performs I/O and dispatches one managed request at a time.

Verified source facts:

* Ghidra `trace-rmi.proto` is language-neutral proto3. Handler framing is a
  four-byte big-endian length plus `RootMessage`; maximum payload 65536 bytes.
  Negotiation requires `12.2`. `XRequestInvokeMethod`/`XReplyInvokeMethod` have no
  invocation IDs; handler replies are correlated FIFO. Launcher `runInTerminal`
  executes an argv array and `launchBackEnd` receives an endpoint, not a Python API.
* `starwing/lua-protobuf` pin `ee58d1a1b6df86e911f1d44194ac68247b97a93b`: inspected
  `pb.c`/`protoc.lua` support table encode/decode, oneof, proto3 optional and loaded
  descriptors. Use the small native codec with Lua-owned schema/actions/projection.
  MIT notices retained. Exact integration interoperability is still unproven.
* luaposix 36.3 pin `e6b94b37c19c4bd19a7dc475a4f4cb56f61f5da9`: inspected socket,
  poll, unistd and wait sources supply TCP/socketpair/connect, nonblocking descriptor
  I/O, fork/exec/dup2, process groups and exact child wait. Prefer this one poll
  domain over adding LuaSocket plus a separate POSIX process/FD layer. No custom
  native debugger or Java transport agent is needed.
* lua-repl-mcp pinned/current HEAD `0b8d271911c17ce762246afa86b74ab20e999343`:
  manager is multi-session evaluator/result policy; guardian owns supervisor process
  lifetime; supervisor imports old `worker_protocol` for allocation/execute/close
  and parsing; `worker_setup` can emit old startup records. They cannot serve the
  new typed union unchanged. Reuse bootstrap/limits/framing/codec and scripting
  evaluator, plus compatible low-level OS helpers. Do not reuse manager/guardian/
  supervisor/setup as the product client or patch their installed bytes. The new
  single-child Lua host uses POSIX primitives directly and owns its own records.

The product has no Python process, package, adapter, generated client or runtime
dependency. Python reference sources in Ghidra and existing regression scripts
are historical/source evidence, not an integration runtime dependency. New product
launcher and wire qualification must run through Lua without importing them.

| Linux R1 lifecycle component | Exact responsibility |
|---|---|
| Java launch offer | Start host directly with ProcessBuilder; retain Process; drain launch control/output; request graceful disposal or forcibly terminate hung host; observe host exit |
| External Lua host | Single-threaded child creator; FD3; child-armed report; healthy disposal/TERM→KILL and exact waitpid; no daemonization or creator-thread replacement |
| Small native pre-exec binding | Fork child arms PR_SET_PDEATHSIG(SIGKILL), compares getppid with expected parent captured before fork, establishes child session/group and execs ordinary DOSBox; error/mismatch exits before exec |
| Linux kernel | Deliver SIGKILL on creator-thread death after arm; no surviving host code or Java child descriptor required |

The binding is lifecycle setup only, not a debugger, PID registry, resident guardian
or pidfd/SCM_RIGHTS transfer mechanism. Other OS support requires a separately
designed and qualified equivalent; POSIX modules alone do not establish portability.

## 3. Identity and state machines

### 3.1 Identity values

Wire integers that can exceed JavaScript's exact range are canonical unsigned
decimal strings. Native pointers, Lua userdata and borrowed frames never cross.

* `session = {allocation, attachment}`: allocation is the fresh launcher bootstrap
  generation scoped to this owned process/channel; attachment is the existing
  native attachment serial allocated before readiness. Neither alone is a global
  identity. No reconnection makes this tuple usable again.
* `operation`: native `Execution` operation serial, meaningful only with session.
* `revision = {invocation, checkpoint}`: exact existing native stopped-context
  facts, always paired with session. A value copied at departure is provenance,
  not current read authority. Unavailable revision is explicitly null.
* `event = {operation, ordinal, registration, invocation, checkpoint}`: native
  provisional hit identity, never interchangeable with an ordinary revision.
* Registration carries both Lua registration ID and exact native breakpoint ID;
  load identity is native observed load-lifetime identity, never filename/CS.

The native attachment is established at the admitted stopped startup seam.
Successful managed `ready` returns this session, initial revision, capabilities
and stopped readiness before any Start. Process-created, FD-connected and booting
are launcher facts; none is a ready target. Startup failures before admission have
no invented session identity. R1 creates its trace only after ready.

### 3.2 Session lifetime

```text
Spawned → initializing → Live(session, initial stopped revision)
                            │ known native departure (atomic publication)
                            ▼
                  DepartedDelivery(operation copy OR session-only fact)
                            │ exact copied release OR explicit close
                            ▼
                     Closing → WorkerClosed → Joined/resources closed → Exited

Live → explicit close/peer loss → revocation + native containment → Closing
Any channel ambiguity → client Poisoned → external disposal only
Any process state → DisposeRequested → DisposedConfirmed | DisposalUnconfirmed
```

Transport health, native state, worker closure and process disposal are independent
fields. A live-session Close must settle/revoke the native owner and acknowledge
discarded result delivery; it is not nondestructive detach. Close may be requested
while Pending, but its destructive lifecycle semantics do not create Pending Pause.
No normal child exit occurs merely because a native terminal fact was transferred.
There is no automatic departure-delivery timeout.

### 3.3 Operation slot

```text
Empty --exact stopped Start--> Pending --normal activation--> Active
                                  │                          │
                                  │ lifecycle loss           ├─ hit → EventPending
                                  │                          │         ├─ suppress → Active
                                  └──────────────┐           │         └─ propagate/error → stop
                                                 ▼           ▼
                                              Terminal (held, or departure)
                                                 │
                        ordinary exact release ──┴──> Empty (same live session)
                        departure transfer ─────────> copied delivery; native slot retired
                        copied exact release ───────> delivery consumed; session closes
```

EventPending is a projection of the one Active operation with a held provisional
event, not a second `Execution` operation or ordinary STOPPED context. Pending can
be observed without dispatching guest execution. Release of Pending/Active/event
is refused; Close is the lifecycle escape. Only normal activation advances Pending.

| Request / state at native consumption | Empty/held | Pending | Active | EventPending | Held Terminal | Departed |
|---|---|---|---|---|---|---|
| Start exact revision | admit | busy | busy | busy | release required | refuse before evaluator |
| Observe exact operation | unknown/released | Pending | Active | event identity | same terminal | copied terminal only |
| Wait exact operation | refuse | bounded state wait | bounded state wait | return event immediately | return immediately | copy immediately |
| Pause exact operation | refuse | **refuse, no activation** | request stop | request stop, settle event through native arbitration | return terminal fact | refuse; use Observe |
| Ordinary read/register/remove | exact context check | refuse | refuse | refuse | exact held-context check | refuse before evaluator |
| Explicit event service/read | refuse | refuse | refuse without event | exact provisional authority only | expired | refuse |
| Release exact operation | refuse | refuse | refuse | refuse | consume native slot once | consume copy once and close |
| Session Observe | current session facts | same | same | same | same | departure fact; no fabricated operation |

Pause consumption checks full attachment/session/operation identity immediately
before setting the native stop request. A queued old Pause cannot target whichever
slot happens to be current. Acceptance reports `stop_requested`, not STOPPED; the
same operation's terminal observation supplies settlement. A pending refusal is a
complete exchange; a later deliberate Pause after normal activation is permissible.

## 4. Singular native service and callback preservation

### 4.1 Split exchanges from execution

Refactor `Handoff` into an exchange slot with independent session mode and copied
status/event facts. `start` takes the ordinary stopped handoff, validates context,
enqueues native work and completes the exchange with its native ID. It does **not**
wait for settlement or release. `local_driver_operation` selects the retained
native slot until exact release/transfer. Ordinary terminal settlement publishes
the copied response projection but keeps the native result selected.

All new bindings (`start`, `pause`, `status`, stopped queries and explicit event
service) use this service. Fixed protocol Observe/Wait/Release call the same native
service or departure copy directly; no Lua source evaluation is needed. Worker-local
opaque contexts remain available for synchronous scripts. External typed records
pass copied revision tuples through fixed Lua bindings which reconstruct authority only by
matching the existing exact owner/context; there is no table of remotely issued
context handles.

### 4.2 Agent-independent Active servicing

Factor the stop-only submission/consumption/body out of Agent-only code into shared
debugger service. Agent ordinary effects remain in their existing queue and retain
order; its stop consumer and managed Pause call the same native exact-stop operation.
Do not move Agent command parsing, controller, RPC operation catalog or delayed
ordinary commands into the Lua owner.

At the existing emulator-thread Predispatch/PostTick/held frontiers, check a minimal
pending-control flag **before** `LocalDriverFrontier`'s condition-free early return.
Service only copied status, exact Pause, release/close arbitration and event control
appropriate to the current native state. Guest reads/registration effects require
the held frontier and full ordinary admission. The Pending branch explicitly
answers status and refuses Pause without invoking activation or the decoder. The
held-frontier owner then performs the normal activation as a separate turn, not
as a side effect of Pause/Observe. Specifically preserve the existing enqueue →
`local_driver_preparing=true` → `DEBUG_DispatchNativeExecution()` → clear-preparing
sequence at `LocalDriverFrontier` (debug.cpp:6357–6371). Publish/complete the Start
exchange after enqueue, service any already-present Pending control without
activation, then take that preparation turn. `DEBUG_DispatchNativeExecution`
(6465–6484) admits this exact held owner only under the preparation flag and selects
`NativeBoundary`; activation failure settles a failure, never leaves an ownerless
Pending slot. Start's reply may still be in transport when activation happens; it
records admission, not a promise that a subsequent Observe sees Pending. A controlled
native test barrier creates a Pending opportunity; transported Observe may still
see Pending, Active or Terminal depending on release and consumption order. Directly
verify at the held native seam that a control consumed while Pending refuses Pause
without activation. The UI gate is truthful projection, not a deterministic Pending
sample or a minimum externally visible Pending duration.
The frontier loop cannot wait for an outer dispatch to activate: it has not returned.
After activation it releases the hold to ordinary dispatch. Separately move the
Pending return check at `Normal_Loop:574–576` outside the Agent-only gate when managed
debugging is built; keep `AGENT_BridgePump` gated. This preserves outer dispatch for
its ordinary callers without pretending it solves held-frontier activation.

Native status changes publish immutable copied observations under the handoff mutex
and notify its condition variable. Wait sleeps on those observations with a bounded
deadline, returning the freshest copied state available at its observation point;
it never holds the mutex while requesting emulator work. Exact Release is consumed
on the emulator thread while live, including when racing departure. Worker wake
and native service are separate: waking FD3 cannot by itself stop the CPU.

The shared service must be compiled with managed Lua and `C_DOSBOX_AGENT` disabled.
Condition-free Run uses ordinary continuous dispatch, not repeated bounded Phase,
new stop conditions, polling instructions or another interpreter loop. Measure the
flag check cost and actual control service order; no per-instruction latency promise.

### 4.3 Existing live bindings and synchronous callers

Migrate `Debugger_LiveResume`, `LiveStop`, `LivePhase` and their completion projection
to native Start/Pause/Observe/Release under the same selected attachment. Existing
live dispatcher continuations yield and later consume the result; they must not
block inside `LiveSlice` or call a worker-only wait. A live frontend and managed
worker are alternative selected callers, not simultaneous attachments. Remove the
independent raw resume/phase execution bookkeeping once callers migrate.

`dosbox.run/step/over/run_until` become synchronous convenience over:

```text
start exact stopped context
repeat bounded wait on same operation
  if event-pending: explicitly service exact event on sole scripting worker
until terminal
copy terminal; exact release; return copied terminal
```

Only the caller blocks. Native/manual/user breakpoint stops retain valid continuation
where permitted; they must not flow through `Debugger_LiveError`'s permanent error
latch. Actual errors/lifetime loss retain their truthful failure semantics. Preserve
unrelated live input/capture and legacy read semantics; Ghidra uses the safe managed
inspector, never legacy `Debugger_ReadMemory`/`mem_readb` as fallback.

### 4.4 Provisional native event lifetime

Choose the smallest safe lifetime: retain the **actual native hit stack frame only
while its provisional event is pending**, as existing `LocalDriverObserveHit` does,
but decouple event storage and service from an executing admission evaluator.
This is a bounded native hit service wait, not a CPU loop and not the post-departure
delivery window. No guest decoder, timer progression or arbitrary command dispatch
occurs inside it. It services exact provisional reads, a verdict, Pause/close and
host lifecycle signals. Only the sole Lua worker explicitly services the callback.

The event stores its existing registration reference until verdict consumption;
removal is refused while Active/event-pending. Worker event service checks identity,
sets callback-only admission and invokes the protected callback with existing
cooperative budgets and effect/coroutine guards. Only Read/TextSnapshot at that
event's native context are admitted. General effects, recursive execution and
ordinary stopped access fail and force a stopping error verdict.

Use a fixed **30000 ms monotonic native event-service deadline**, starting when the
native event is published. It is a DOSBox contract constant, advertised as
`ready.event_service_timeout_ms`, not configurable through the seven dependency
limits. On expiry, error, close or departure request the native hit owner
invalidates event read/verdict admission, completes/refuses any pending read,
settles error/stop and returns from the hit frame. A late callback cannot supply a
verdict or regain read authority. The frame is never unwound while reads can still
use it. A callback blocked in foreign C may continue host work; native expiry does
not claim to preempt it. External disposal remains necessary for a stuck worker.

Pause during event-pending requests a stop and revokes provisional authority through
this same arbitration; it does not wait for a suppressing callback to permit stop.
Before the hit frame returns, all waiter-visible outcomes have been copied and
borrowed authority removed. Departure proper then unwinds native frames and uses
only the terminal copy. No terminal delivery waits inside the hit frame.

R1 selects a callback-free external profile at startup. Native Start refuses when
any callback-bearing registration/predicate is armed. Stage 0 must nevertheless
prove existing scripting callback suppression/propagation/read/close behavior on
this singular implementation before replacing the old synchronous engine.

`Execution::eventContext` increments the checkpoint. Publish that resulting exact
event identity and invalidate earlier ordinary context observations; callback reads
use only this provisional identity. Pause is session/operation-addressed, not
checkpoint-addressed: it arbitrates against the selected Active event at consumption.
Do not invent an event-checkpoint requirement for Pause.

## 5. Departure ordering and delivery

The native service is the serialization owner for live release, departure and
live effect admission. Session dispatch decisions and copied delivery mode share
its handoff mutex. A live execute receives an admission ticket only while Live;
every resulting binding still checks mode and exact native authority at consumption.
No lock is held across Lua evaluation, native teardown or socket writes.

At departure: revoke live and provisional authority; settle an unresolved selected
operation once; copy the existing terminal (if already held, preserve that terminal's
outcome/reason) plus a separate departure fact and `continuation=false`; switch
mode; retire native result exactly once; clear exact owned registrations/attachment;
unwind. Never overwrite a known held stop with an invented exit cause.

The **native departure seam** remains the settler: selected-program ExitFence and
restored-root close call one native transfer helper before their owner/frame
unwinds. That helper settles the selected unresolved Lua `NativeBoundary` operation
with the actual departure reason, or preserves an existing Terminal, then copies,
releases native retention and revokes/detaches exactly once. Keep the
`Execution::ownerDeparting` exemption for an Active Lua `NativeBoundary`: generic
owner departure does not settle it in place of the specific seam. Pending and
ordinary operations retain their existing owner-loss settlement. Move only remote
delivery wait, Lua join and descriptor retirement out of ExitFence, never the
settlement that needs its live owner. Top-level teardown cannot reconstruct a
reason after unwinding and never calls the retired execution owner.

Replace today's `terminal() => closing_=true` and `sessionStopping()==closing`
coupling. Native `Live/DepartedDelivery`, transport `Open/PeerClosed/Failed`, and
worker `Serving/Closing/Closed` are distinct state fields. Departure changes only
native mode and publishes the copy; it does not make the dispatcher stop predicate
true. The DOSBox dispatcher replaces dependency `session.serve`, whose present
`host_stopping` behavior is unsuitable. Stop admitting requests only on explicit
Close, consumed departed Release after reply flush, or transport failure/disposal.
The current admitted reply remains flushable. All new live typed calls and Execute
receive departed refusal; fixed observation continues until consumption/Close.

| Ordered winner | Committed response / retained delivery | Subsequent behavior |
|---|---|---|
| Live Release first | ordinary release response is immutable; native slot empty | departure without successor publishes session-only fact; never resurrect released operation |
| Departure transfer first | copied terminal selected; native storage retired | exact Release consumes copy, replies `released:delivery`, then closes worker |
| Wait observes Active first | immutable Active reply | later fixed Observe/Wait returns terminal copy |
| Execute dispatch sees Departed | complete `departed` refusal, no evaluation | fixed observation allowed after complete matching refusal |
| Execute admitted before departure | current evaluator may finish/refuse bindings; known effects retained | flush its one reply; no new evaluator admission |
| Session-only departure | `{session, departure, operation:null}` | session Observe/Wait and Close only; operation Release refuses |

An already-decided reply occupies the current exchange reply buffer until send
completion or fatal transport loss. Departure cannot replace it with another reply.
Native teardown never waits for remote receipt. Response consumption/flush is not
proof of client receipt; only a full matching client exchange is receipt evidence.

Change `sessionRead` to wait on FD3 **and** one local eventfd/self-pipe owned by the
managed session. Native mode changes signal that local descriptor and the wait
condition variable. Replace the borrowed read ABI for this DOSBox dispatcher with
one tagged result: `{kind="data", bytes=nonempty_string}`, `{kind="wake"}`,
`{kind="eof"}`, or `{kind="error", code=bounded_code}`. Native polling consumes the
local wake signal and returns wake; Lua rechecks mode and resumes the SAME framing
buffer. Neither nil nor an empty data string means wake. EOF with a partial frame
is truncation/failure. Pinned framing is reused only behind an adapter that retains
its buffer across wake; dependency read/serve semantics remain unchanged.
Keep partial incoming frame bytes intact. Never inject response bytes, return
spurious EOF, call current destructive `wake()/shutdown(SHUT_RDWR)` on departure,
or create a second reader. Shutdown remains a destructive close/disposal mechanism.

Move delivery wait/join from ExitFence and restored-root borrowed frames to the
top-level managed-session owner after native unwinding. It retains copied facts,
worker and descriptors only. Fixed terminal records do not call the retired native
owner. Exact copied Release sends and flushes its response before worker closure;
Close acknowledges any discarded copy. Finalizers run with guest admission revoked.
WorkerClosed is published after `lua_close`; only then join and close owned
descriptors/finish ordinary child exit. Peer loss can lose receipt and triggers
external containment; no automatic receipt retry or delivery journal.

Release ordering is consume → construct matching success → write/flush → (for
departed release only) close worker. Live release consumes the native slot on the
emulator thread; departed public release consumes only the already-transferred
copy, never a retired native result. Failed validation consumes nothing and returns
a complete refusal. Failure before a known consumption is reported as such;
construction/write/flush failure after consumption poisons the channel and may lose
the receipt. Do not undo consumption, resend Release, probe status or synthesize
success. The client preserves its previously copied terminal facts but marks release
receipt unknown and disposes externally. If response flush succeeds but Lua close,
join or process exit fails/hangs, release remains known consumed; disposal remains
unconfirmed until exact exit evidence. No successful reply promises finalizers or
child exit. This is deliberately not exactly-once delivery or a recovery protocol.

## 6. DOSBox managed wire contract v1

### 6.1 Envelope and records

UTF-8 JSON, one newline-terminated object per record; use pinned framing/codec,
bootstrap, evaluator and applicable value/limit validators as libraries. DOSBox
owns strict union validation and dispatch, not a copy of `worker_protocol`.

Every record has `protocol:"dosbox-managed"`, `version:1`, `type`,
`allocation_generation`. After ready every request/reply also has `generation`
(strictly increasing unsigned positive integer represented canonically) and the
exact `session`. Unknown fields/types, duplicate keys, invalid numeric encodings,
wrong protocol/version/allocation or unexpected generation are fatal framing/schema
failures, not compatibility probes. Use a decoder that detects duplicate keys;
this must be checked in library selection rather than assumed from JSON syntax.

| Record | Required payload / meaning |
|---|---|
| Host `ready` | generation 0; session; `state:"stopped"`; revision; capability/build/profile facts and effective limits; no operation required |
| Client `start` | exact revision, `kind:run|step|over|run_until`, validated per-kind options; fixed Lua binding; returns Pending/Terminal native identity |
| Client `pause` | exact non-null operation; fixed binding, native consumption-time checks |
| Client `context` / `registers` / `read` | context/status or exact revision and bounded address/range; no user code; safe native reads only |
| Client `breakpoint` / `remove` | exact revision, supported execute address or exact registration/native ID; no callback/code field in external R1 profile |
| Client `execute` (trusted scripting only) | `input:{code,args}` with pinned evaluator encoding; never selected/sent by the Ghidra host; cannot bypass native/profile/lifetime admission |
| Client `observe` | `operation` decimal ID or null for session observation |
| Client `wait` | same target, `bound_ms` integer 0..250; wait for changed state or deadline, event/terminal immediate; expiry returns state, not cancellation |
| Client `release` | non-null operation ID, no arbitrary input |
| Client `close` | no operation needed; explicit discard/closure request |
| Host `response` | same request generation/session; `request_type`; `status:"ok"|"refused"|"error"`; strict payload union below |

Typed live and observation records return native/session projection directly through
fixed Lua binding dispatch, with no source evaluation. Trusted scripting Execute
responses retain the pinned evaluator result encoding; they are outside the Ghidra
method path. A normal
typed refusal carries `{code, effect:"none", current_state}`; post-admission errors
instead carry known operation/effect facts and `completion:"known"|"unknown"`.
Do not label evaluator errors automatically no-effect: earlier bindings may have
completed. Diagnostic strings are bounded and do not dump code, arguments or paths.

Operation observation union:

* Pending/Active: session, operation, state, available start revision; no invented
  current registers. EventPending adds the exact event identity and deadline facts.
* Terminal: session, operation, `outcome:stopped|exited|failed`, `raw_reason`,
  normalized reason, nullable end revision, register validity, continuation,
  available copied register/hit/exit/budget facts, and `storage:native|delivery`.
  Delivery adds departure facts. Unavailable values are explicit null/validity false.
* Session observation: mode, capabilities, selected operation identity if any;
  session-only departure has null operation and known/unknown departure facts.
* Release: exact operation, `released:native|delivery`, `closing:boolean`.
* Close: `close_accepted`, discarded delivery identity if any, known native facts
  and `process_exit_confirmed:false`. Process owner reports exit separately.

No response shape confers read authority just because it contains revision data.
Exact IDs are compared as values, never parsed as code or used for function lookup.
All fixed records have defined admission before and after departure; live-only
records refuse after departure. Terminal `execute` always refuses before evaluation,
including code that would only read a cached Lua value.

### 6.2 Ordering and bounds

Only ready is unsolicited. Then there is exactly one request and one response in
flight. All records, including Close and fixed controls, consume the next generation.
The host reserves it only after full schema validation; a valid typed refusal also
consumes it. The client advances only after one complete validated matching reply.
Neither wraparound nor generation recovery is supported. Trailing unexpected records
are protocol errors, not queued unsolicited status updates.

Preserve all seven bootstrap budgets from the pinned limits contract:
`input_bytes`, `values_bytes`, `diagnostic_bytes`, `max_depth`, `worker_bytes`,
`response_bytes`, `output_bytes`. Validate them with pinned `limits.connection`,
including its coupled capacity floors and overflow checks; they are not seven
independent unchecked knobs. Cap each
record at `worker_bytes`, recursively encoded values at `values_bytes/max_depth`,
diagnostics at `diagnostic_bytes`, and evaluator code/argument/output at their owning
limits. Ready advertises the actual limits; a package whose mandatory ready/result
cannot fit is rejected at launch. Native memory read maximum is advertised (currently
4096); integer/address/range arithmetic is checked without overflow. Protocol Wait
maximum is 250 ms; the Lua host normally uses Observe or shorter Wait and does not
pipeline. These bounds are not hard real-time guarantees over blocked native/foreign
code or transport. Callback service has its separate explicit budget (§4.4).

The host parser/serializer must not truncate a terminal or reply to fit. Where a
prospective request's response size can be bounded, reject it before effects; if
reporting fails after known admission, retain known facts and fail the channel
rather than manufacture a success. Byte transfer loops advance through positive
partial writes; EINTR handling follows the existing socket contract. No whole
logical record is resent after uncertain completion.

### 6.3 Typed live bindings and affected clients

Ghidra's typed method selects a fixed Lua host action that creates a typed managed
record. The sole worker's DOSBox dispatcher validates it, selects a compile-time
fixed binding and passes only data. Address, path, operation/revision/options and
breakpoint coordinates are never Lua source or user-selected function names.
The current union also retains trusted scripting Execute for existing non-Ghidra
use; if tooling uses it, use constant owned chunks plus separately encoded args
(the pinned evaluator passes args as varargs). It is not a second serving loop or
fallback, and the Ghidra host never sends it. The external R1 callback-free admission
restriction is native-enforced. All live typed records and new Execute refuse after
departure before binding/evaluation. R1 registers no arbitrary code, EXECUTE action,
console forwarding or general evaluator method.

Update DOSBox-specific `managed-session.py`, `managed-session-mcp.py`,
`managed-session-saivage.mjs` and worker fixtures wherever they assume the old host
records. Existing stock lua-repl-mcp manager protocol clients are **not** magically
compatible with this new custom host. Their DOSBox integration must use the new
DOSBox client endpoint/launcher contract explicitly; retain tests of dependency
ordinary sessions separately. No custom-host claim remains backed by the old
manager decoder. A named thin DOSBox client bridge may serve an existing test
harness, but cannot be a second serving loop or production MCP requirement.

Ship host/client schema, dispatch, focused tests, current managed-session docs and
package/notices together in Stage 0. Preserve original dependency pin/tree/archive
notices. The artifact version is DOSBox's, not a fictional lua-repl-mcp release.

Existing Python regression fixtures above are test clients, not shipped integration
runtime components. Preserve their coverage while updating the custom-host union;
do not delete them merely because the product host is Lua.

### 6.4 External Lua Trace RMI wire and declaration ownership

`trace_wire.lua` loads a build-produced descriptor of the exact pinned
`trace-rmi.proto` through `pb.load`; the dependency JSON codec is **not** a protobuf
codec. Set `pb.option('int64_as_string')`. Convert checked canonical decimal values
to/from the codec's `#`-prefixed strings, preserving signed int64 Snap/Span and
unsigned uint64 Addr offsets without floating-point conversion. Lua owns validation
of oneof choices, optional presence, enum values, range arithmetic and expected
reply type. Proto3 unknown-field skipping is normal codec behavior, not a promised
strict wire-field rejection; unknown/absent root variants and unsupported semantic
values fail. Version mismatch is fatal, not a probe for another schema.

TCP framing is `string.pack('>I4', #payload)` then protobuf RootMessage, payload
1..65536 bytes. Incremental readers retain partial header/body; reject oversize
before allocation. Writers retain byte offsets over partial sends and retry only
proven non-transfer/EINTR or readiness conditions; never resend an uncertain frame.
Check encoded size, including envelope overhead, before sending. FD3 uses its
separate newline JSON framing and bounds. The host poll set contains Ghidra TCP,
parent socketpair endpoint, and launch lifecycle control; native local wake stays
inside DOSBox and is not another external socket. Use nonblocking TCP/parent FD3,
bounded per-turn work and output buffers. Queue overflow fails the connection and
initiates containment, rather than unbounded memory or blocking child settlement.

The Lua host sends RequestNegotiate(version="12.2", methods=the closed table below,
description=package identity), waits for ReplyNegotiate, and creates no trace before
native ready. It owns RequestCreateTrace(path, language, compiler, oid), then
RequestCreateRootObject with packaged XML `schema_context` and root schema name;
root schema is **not** a CreateTrace field. The supported outbound subset is
CreateTrace/CloseTrace/SaveTrace, StartTx/EndTx, CreateOverlaySpace, Snapshot,
CreateRootObject/CreateObject/InsertObject/RemoveObject/SetValue/RetainValues,
SetMemoryState/PutBytes/PutRegisterValue, and Activate, with their exact replies
(CreateRootObject shares ReplyCreateObject). No remote disassembly, GetValues-based
history cache or generic wire passthrough is needed. ReplyError fails the pending
transaction/request; do not pretend earlier acknowledged mutations rolled back.

The host chooses `oid:DomObjId{id:uint32}` before CreateTrace; R1's sole trace uses
id 1 on its fresh connection. ReplyCreateTrace is empty, not an assigned/echoed ID.
Retain and supply that same oid on every trace-scoped request, including root,
transactions, data, activation, save and close. Successful create establishes open
use; an ambiguous create poisons the connection without probe/retry. Reverse method
invocations require this exact open oid. Closed/superseded IDs are never reused on
the connection; future additional traces use unused uint32 IDs, failing on exhaustion
rather than wrapping. Close stops new invocations/publication, settles or explicitly
abandons pending work, and retires the ID on successful CloseTrace; connection loss
leaves closure unknown. Saved database identity is not this connection-local ID.
Connected trace Close also initiates §10 disposal, not a claim of child exit.

Only one ordinary host-to-Ghidra request awaits its corresponding reply. Independently
receive XRequestInvokeMethod while waiting, validate oid/name/arguments against the
declared table, and eventually emit XReplyInvokeMethod(error or typed return).
Reverse requests have **no IDs**: retain a bounded arrival-ordered reply queue matching
Ghidra's `xReqQueue`. Interrupt may overtake unsent refresh work on FD3, but its reply
must not overtake an earlier method reply. Complete obsolete refresh as unavailable
or finish its bounded current exchange before draining replies in arrival order.
Never reorder already-dispatched effects. This dual-direction bookkeeping is local
socket state, not a second native operation catalog. Close/error abandons outstanding
calls explicitly; it does not replay them on a new connection.

`schema.xml`, shipped by the Lua package, owns an actual `<context>` of named
`<schema>` definitions with interface declarations, canonical container `<element>`
schemas and typed `<attribute>` declarations for every §8.1 path. Root name is
`DOSBoxSession`; container schemas reference Process, Thread, StackFrame,
RegisterContainer, MemoryRegion, Module, Breakpoint and Segment schemas. `_state`
is STRING, `_pc` ADDRESS, `_range` RANGE; `_focus` and `_event_thread` are object
references to the declared schemas. Declare required standard interface attributes
and element traversal, not a bag of JSON properties. `Processes[guest]` is a path
under the root (the root schema name is not a path prefix). Lua sends the XML bytes
in CreateRootObject; Java parses them and consumes interfaces. XML/parser and actual
GUI binding tests must qualify the complete generated artifact before delivery.

Lua action-table entries generate protobuf `Method` and `MethodParameter` values:
name, exact action hint, display/description, ordered required typed parameters,
return_type and optional UI metadata. Schema names are `ValueType.name`, not Lua
class names. All following R1 methods return VOID (successful null Value); all
parameters listed are required, with no defaults. Object arguments must belong to
the exact trace/session; range/address arguments are additionally native-validated.

| Name | Action hint | Parameters (name: schema) | Fixed owner/action |
|---|---|---|---|
| refresh_environment | refresh | node: Environment | Lua host publishes native facts |
| refresh_registers | refresh | node: RegisterContainer | stopped registers |
| refresh_memory | refresh | node: MemoryContainer | safe interval projection |
| refresh_breakpoints | refresh | node: BreakpointContainer | current registration projection |
| read_mem | read_mem | process: Process, range: RANGE | bounded safe read |
| activate | activate | object: OBJECT | trace focus only |
| resume | resume | process: Process | typed Start(run) |
| interrupt | interrupt | process: Process | typed Pause(exact operation) |
| step_into | step_into | thread: Thread | typed Start(step) |
| step_over | step_over | thread: Thread | typed Start(over) |
| break_execute | break_sw_execute | process: Process, address: ADDRESS | native execute breakpoint |
| delete_breakpoint | delete | breakpoint: Breakpoint | exact native removal |
| terminate | kill | process: Process | owned lifecycle disposal |

These strings bind the pinned ActionName conventions, not arbitrary labels. XML
includes Environment/MemoryContainer/BreakpointContainer and a Breakpoint carrying
the §8.1 interfaces so schema traversal can resolve these signatures. There is no
untyped catch-all method or arbitrary evaluator argument. Validate binding against
12.2 method matchers and actual standard actions; declaration alone is not proof.
New stage-specific methods require equally explicit declarations, not registration
of unsupported R2 actions in R1.

Every memory request explicitly includes snap; AddrRange uses `extend=length-1`,
checked for nonempty range and uint64 overflow. Every object value uses its explicit
Span, including signed endpoints. Register `RegVal.value` is a binary Lua string
of exact-width big-endian bytes (not JSON numbers, decimal text or target-endian
memory bytes); use checked shift/byte assembly without a floating-point intermediary.
Create the register overlay before publication and inspect ReplyPutRegisterValue's
skipped names. These encoding checks and FIFO dispatch are package-owned tests,
not assumptions inherited from the Python client.

## 7. Native observations, addresses and instrumentation

One exact stopped response projects registers, native mode admission, capability
facts, revision, selected load and safe memory intervals. Read and instrumentation
consumption validate owner and revision anew; no whole-history validation, new
currentness registry or adapter-generated stop clock.

Ordinary-real-mode admission is produced natively from current mode/VM, relevant
loaded segment base/limit/validity/default-size and paging interpretation. Visible
segments plus `!pmode` are insufficient. For R1 only qualified ordinary RAM below
`0x100000` is advertised; exclude device/ROM or otherwise unqualified intervals.
Expose raw available registers/cache facts even at an unsupported stop, mark
address-sensitive capability false, remove misleading frame-PC/decoding/navigation
and refuse new address-sensitive effects. Preserve lifecycle controls and safe
control observation/Pause; do not add continuous mode-transition surveillance.

Trace `ram` always means the qualified linear observation view. Native inspection
uses `PAGING_InspectLinear` and safe backing primitives, never `mem_readb`, guest
faulting translation, segment reload or device handlers. Below-1-MiB admission is
not a universal A20/TLB/cache claim. Safe interval projection comes from native
memory handler/layout knowledge at the stopped revision, not discovery by failed
byte reads. Permission to inspect is not guest permission to access.

For a flat read, split on advertised native maximum, safe interval ends and known
native page boundaries. Subread failures contribute zero bytes. Aggregate success
ranges and explicit holes; a 4096-byte GUI block is covered by all bounded pieces,
not a truncated prefix. If safe subdivision is unknown, report the block unavailable.
Do not probe bytewise. Explicit logical reads use a distinct native request with
segment/register and offset; R1 chooses **refusal of offset-wrap-crossing requests**
(offset + length > 65536), rather than silently treating them as flat reads. Flat
reads remain linearly advancing. Preserve requested logical coordinates separately.

Breakpoint creation accepts a flat mapped address in qualified R1 memory. Native
conversion chooses a real-mode segment:offset representing exactly that linear
location, calls the existing resolver and verifies equality before creation. It
does not preserve an invented selector predicate. Native stop production records
the actual breakpoint ID before reducing hit status; join to the existing Lua
registration at that producer, with owner/kind/internal/temporary and available
count. Internal Over/run-until ID cleanup uses its exact owned ID. If multiple
hits are actually reported, preserve those facts; never infer a user hit from PC.
Deletion uses exact IDs/context and expires the trace location on known success.
Unavailable hit attribution is explicit. No toggle via delete/recreate.

Native launch selection captures the actual COM PSP, entry, image extent and
load-lifetime facts before arbitrary guest execution. Reuse startup program-selection
and DOS load ownership seams; add only the necessary copied load projection. Do
not infer load identity from current CS, pathname or adapter counters. Unsupported
child/overlay transitions invalidate claims of current module/mapping rather than
silently applying the old load identity to reused addresses.

## 8. Lua integration host and trace model

### 8.1 Model and actual consumers

Use Lua construction/validation of the pinned Trace RMI messages (§6.4), not Python
`ghidratrace` classes. Proto `Addr`, `AddrRange`, `RegVal`, schema XML and typed method
definitions map directly to Lua data. Use canonical schema containers/aliases:

| Canonical path | Interfaces / content |
|---|---|
| `DOSBoxSession` | EventScope, FocusScope, Aggregate; session/protocol/build/profile, completeness, `_focus`, `_event_thread` |
| `Machine` | observed mode/cache/A20 validity and limitations; no synthetic execution thread |
| `Processes[guest]` | Process, ExecutionStateful, Aggregate; one controlled guest context, not a DOS scheduler claim |
| `Processes[guest].Environment` | Environment, actual architecture/OS/debugger descriptions |
| `…Threads[cpu0]` | Thread, ExecutionStateful, Aggregate; sole guest CPU |
| `…Threads[cpu0].Stack[0]` | StackFrame, `_pc` ADDRESS = native effective instruction location when admitted |
| `…Stack[0].Registers` | RegisterContainer and its register overlay space; actual bank values |
| `…Memory[key]` | MemoryRegion, `_range` and factual read/executable properties; inspectability separate from guest permissions |
| `…Modules[load-id]` | Module, `_range`, `_module_name`, actual PSP/load/entry provenance and lifespan |
| `…Breakpoints[lua-id]` | BreakpointSpec/BreakpointLocation with `_range`, `_enabled`, `_kinds`, exact native identity/hit metadata |
| `Segments[name]` | raw visible selector and loaded cache/validity; custom observation objects, not translation engines |

R1 needs no artificial sections when the COM module range maps directly. R2a adds
actual Section objects. Descriptor-table objects and multiple frame unwinding are
not R1 placeholder features. Initial transaction publishes and then activates the
real cpu0/frame0, so standard Registers, Stack, Listing, Modules, Memory Map and
Breakpoints windows bind through interfaces, not just custom properties.

Select `x86:LE:16:Real Mode` / `default`. Translate native integer registers to
language-matched **big-endian register byte arrays** of exact widths. Memory bytes
remain target little-endian. Check `put_registers` skipped names and fail the
claimed register projection if required registers are absent. Do not flatten EIP;
frame-zero PC supplies effective CS:EIP for normal Auto PC and far transfers.
Explicit stack navigation uses the native admitted SS:SP effective address and
retains logical coordinates. No fabricated callers, locals, FPU/control banks.

### 8.2 Scheduling and time

One scheduler serializes FD3 requests and observation batches. UI refresh demands
are coalesced by current stop/range, not queued as a mandatory backlog. While
Pending/Active, refuse ordinary stopped refresh locally or return unavailable
without sending guest reads. After the one in-flight short exchange, service an
explicit Interrupt ahead of refresh demand; never reorder two already-dispatched
effects. Recheck state/identity when selecting each request. No FD3 worker pool.

Keep only the current native revision → current live snapshot association in adapter
memory; history lives in Ghidra. Publish a new snapshot for meaningful native
activation/stop transitions, not every identical poll. Pending has explicit custom
operation state and no false RUNNING claim; use non-running/unknown standard state
where no exact enum exists and disable effects until appropriate. Active projects
RUNNING; Pause-requested does not project STOPPED. Held terminal projects STOPPED
only when native continuation/context facts support it. Departure/host disposal
states and causes remain separate.

At new activation/stop snapshots explicitly set advertised RAM ranges unknown
before any successful current capture, preventing inherited known bytes. Preserve
historical byte values at old times. At stop capture essentials first (registers,
mode/frame PC, bounded PC bytes and selected stack/data), then demand-driven memory.
Mark actual read failures error/unavailable; never mark unrequested ranges error.

A batch captures one session/revision, serializes against effects, checks every
response identity, and publishes with explicit snap arguments and `Lifespan.at`
or justified factual object lifespans. Historical cursor is never a publication
target. If current stop changes unexpectedly, discard uncommitted mismatched
observations rather than retarget them. An old-time Refresh may enrich only the
still-current live-stop snapshot. If there is no stopped live context it refuses.

Open trace transactions only **after** native request completion and close them
after a bounded observation update. Never hold them across an effect or long Run.
On failure mark incomplete where possible; if even that publication fails report
the missing projection externally. Preserve known effect/terminal facts in the
adapter's current operation record. Fresh stopped reads may repair display, not
replay effects. Release a terminal only after copying its known facts locally;
publication failure does not force indefinite native slot retention or authorize
another Run automatically.

### 8.3 Closed method surface and reasons

Register typed refresh/register/environment/read_mem, focus-only activate,
resume/interrupt/step_into/step_over, execute-breakpoint create/delete and target
terminate. Advertise only selected native capabilities, rechecked at use. Do not
register Detach, Step Out, reverse, target writes, condition/toggle/one-shot,
watchpoint or console/execute methods. Trace Close is a lifecycle hook, independent
of trace Save; launcher Dispose is outside target effect methods.

Resume/step uses current native revision and explicit async Start; interrupt uses
exact current operation. Static breakpoint creation requires selected verified
mapping and supported address profile. Refresh and activation have no time argument
and no execution effect. Normalize stop reason into startup/entry, step, user
breakpoint, manual/Pause, condition, budget (actual unit), cancellation, program/root
exit, fault, owner/attachment/continuation loss or unclassified with raw reason.
Native identity, not Lua-host PC comparisons, determines attribution. Wait expiry
and host exit are not stop causes. A host-only death reports unknown native cause.

Terminal projection is exhaustive over the current native enum: `Held` and `Budget`
map to `outcome:stopped` (Budget retains actual units); `ProgramExit` maps to `exited`;
`Error`, `LogOpenError`, `Exception`, `ContinuationLost`, `OwnerLost` and
`AttachmentLost` map to `failed`. Preserve the exact raw enum plus detailed native
reason, and derive continuation/register validity separately, never from outcome
alone. An already-held terminal followed by departure retains that original outcome
and gets a separate departure fact. A structurally valid terminal with an unknown
raw reason projects `unclassified` plus the preserved raw value; continuation is
allowed only by the explicit native field, never inferred from reason text. Invalid
protocol outcome enums or terminal schemas fail the channel, not normalization to
unclassified. Keep the current native enum mapping exhaustive; unavailable cause
does not imply ProgramExit. Session-only departure has no synthetic terminal.

## 9. Ghidra 12.2 selection guard and consumer boundary

Source evidence rules out a launcher-only guarantee: `TraceRmiTarget`'s nested
action entry calls private `invokeMethod`; enablement uses connection last snapshot
via `getSnap`, not selected time. Memory/register writes and breakpoint create/
delete/toggle contain direct `RemoteMethod.invokeAsync` calls outside that helper.
`TraceRmiHandler` directly constructs `new TraceRmiTarget` (line 874), with no
factory hook at that site. `RecordRemoteMethod.invokeAsync` validates arguments,
then calls `handler.invoke`; the latter queues and writes the request (1258–1284).
Substituting just a docking action/launcher or subclass is not a complete hook.

**Selected packaging boundary:** ship a small auditable source patch to 12.2's
Debugger-rmi-trace consumer dispatch and an extension for launch/navigation/help.
Build an isolated distribution with that patch, record upstream and patch hashes,
and visibly identify it. The patch adds a DOSBox-specific invocation policy at the
target/remote-method dispatch boundary; it does not change Trace RMI protobuf,
generic language semantics or other targets' behavior. The launch offer refuses
to start without the exact installed guard identity. No unguarded stock fallback.

For this package's exact DOSBox root schema/protocol identity, classify the closed
registered methods as read-only or effects. Unknown methods fail closed for this
target. All effect entry paths obtain a call-local selection stamp **at the first
target API/action entry**, before prompts, asynchronous mapping work or queueing:
selected trace identity, full time schedule (must be snap-only), published live
trace/snap, connection/target session identity and applicable current state. Target
API paths include toolbar/keybindings/context actions, logical breakpoint service
create/delete/toggle paths, memory/register edit paths and kill/terminate helpers.
Unsupported paths refuse; their absence in the Lua action table is not the guard itself.

The guard observes selected-coordinate changes using the tool's trace-manager
coordinate events. A per-tool selection revision invalidates outstanding stamps
on any selected trace/time change, including away-and-back. This is transient GUI
queue invalidation, not native authority, a token service or durable registry.
Capture/check runs in the same GUI coordinate event ordering. Effect enablement
checks live selection plus ordinary native-state-derived enablement. Immediately
before request enqueue/write, recheck the stamp, current selected trace/schedule,
target validity and current live identity/snapshot/state. Guard check and dispatch
acceptance form one serialized GUI action boundary; asynchronous work must carry
the stamp, not recapture a new one after queueing. Marshal the short dispatch
acceptance to that ordering, without waiting there for the remote response.

Route all DOSBox target effect sends through this policy; the lowest remote-method
dispatch refuses missing stamps for effect methods. This final check prevents a
missed built-in helper from becoming an unguarded action. Generic read-only
refresh/activation remains usable while historical, with §8's explicit publication
time. Invocation from malicious direct local APIs is outside the operator-selection
trust guarantee, but the shipped ordinary methods still have no intentional bypass.

Selection changing after valid dispatch does not revoke the already sent request.
Native exact session/revision/operation admission remains mandatory independently.
Target Terminate is guarded like an effect. Launcher Dispose always remains
available for its owned child. Deliberate connected trace Close ends its session
and initiates disposal even if history is selected; simply browsing/saving does not.

R1 does not repair SLEIGH `currentCS`/relative target semantics by publishing frame
PC. The extension supplies a persistent target limitation notice and disables or
clearly labels affected dynamic evaluated-target/p-code navigation in its supported
workflow. Static Program analysis remains separate. The implementation's early
consumer proof must show that normal Listing interactions do not offer misleading
branch destinations as reliable. If notice/action restriction cannot achieve that,
add the smallest demonstrated consumer remedy under the approved boundary and
review it before shipping; never claim a tracker fixes branch semantics. This is
an explicit acceptance dependency, not an unattended all-mode language project.

## 10. Launch, mapping, containment and distribution

Launch offer: `requiresImage()==false`, no-task auto-map policy, loopback ephemeral
Trace RMI acceptor. Operator selects DOSBox executable, explicit config and COM;
use argv arrays with no shell interpolation. Adapter creates the socketpair,
passes only child endpoint as FD3 plus exact bootstrap budgets/generation, and
closes duplicate endpoints. DOSBox marks adopted FD3 CLOEXEC. Use the established
`-lua-driver-session -lua-driver-program` stopped-entry route. Launch completes
without Program selection/import/mapping. No BIOS/external-boot workflow is implied.

The Java launch offer starts installed Lua5.4 and its host entrypoint directly with
`ProcessBuilder` argv and retains the returned `Process`, not a shell/PTY wrapper or
a Process rediscovered from a PID. Dedicated launch control uses Process stdin/stdout;
diagnostics use stderr. Java drains both outputs and performs waits off the GUI
thread. These are new launch-offer responsibilities, not stock TerminalSession
guarantees. `ScriptAttributesParser:391–395` supplies
`GHIDRA_TRACE_RMI_ADDR`, `GHIDRA_TRACE_RMI_HOST`, `GHIDRA_TRACE_RMI_PORT`; use HOST/PORT
as the TCP destination and validate consistency with the combined address. These
name an acceptor, not an inherited connected socket. Connect/negotiate before DOSBox
startup so connection and native-ready failures remain distinct. The launch offer
also supplies explicit package paths and the launch-control streams. Do not
assume stock terminal termination alone proves descendant death.

The host uses `socketpair(AF_UNIX, SOCK_STREAM)`, fork and exec of the selected
DOSBox argv. Child setup keeps a blocking child endpoint, dup2s it to 3, closes the
parent endpoint and duplicates (taking care when either original fd equals 3), and
passes `LUA_REPL_MCP_SESSION_FD=3`, fresh
`LUA_REPL_MCP_ALLOCATION_GENERATION`, and `LUA_REPL_MCP_LIMITS` JSON containing all
seven validated budgets. Installed `LUA_PATH`/`LUA_CPATH` are explicit; remove
versioned overrides that would select another module tree. Parent closes its child
endpoint and sets only its own endpoint nonblocking. Child inherits no Ghidra TCP
or lifecycle-control descriptor. DOSBox adopts 3 and marks it CLOEXEC, so guest
tool subprocesses cannot inherit the channel. Setup failure is a launch error,
not an old dependency `startup_error` record inserted into the new union.
In particular redirect child stdin/stdout/stderr away from the Java↔host control
streams to its configured ordinary input/diagnostic destinations before exec;
DOSBox output must never be parsed as a host lifecycle report.

**Selected Linux containment:** the single-threaded Lua host captures its own PID
before fork. In the fork child, the small native lifecycle binding's first setup
operations arm `prctl(PR_SET_PDEATHSIG, SIGKILL)` and compare `getppid()` with that
captured expected parent. Any error/mismatch calls `_exit` before exec or readiness;
checking only `getppid()==1` is insufficient because a subreaper may adopt it. The
child then calls `setsid()` to establish its own session/process group, performs FD
setup, and reports successful arm/parent check over a private setup pipe before
exec of the nonprivileged DOSBox binary. That pipe is setup-only, CLOEXEC, closed
after use, and not a second debugger channel. The host relays `child_armed` to Java
before forwarding native-ready; its report is a setup fact, not exit confirmation.
The expected-parent check closes the death-before-arm race; death after arm triggers
the kernel signal even if the report has not arrived. Failure/EOF before an armed
report aborts launch, never admission of an unverified child. A child not yet
scheduled may only execute this fail-closed setup after parent death, not DOSBox.

Linux applies PDEATHSIG to death of the **creating thread**. The Lua host therefore
forks from its sole long-lived thread and does not daemonize, exec a replacement
supervisor or let that thread exit while considering the child live. Ordinary exec
preserves the setting; setuid/setgid/file-capability exec or credential changes can
clear it and are unsupported in the qualified package. Do not fork another wrapper
between arm and DOSBox: fork clears the setting in that new child. Qualification
checks the selected build/launch path preserves these conditions.

Choose **SIGKILL**, not parent-death TERM: it cannot be caught, blocked or ignored,
and escalation cannot depend on the parent after it has died. This deliberately
provides no cleanup, finalizer, save, result-delivery or rollback guarantee. A task
stuck in uninterruptible kernel work may not exit promptly; signal delivery is not
a bounded-time exit guarantee. No stronger claim follows from killing the host.

Healthy disposal requests one protocol Close only on a healthy FD3 channel, then
waits for the exact child. The Lua owner allows 5 seconds for cooperative close,
then sends TERM to its still-owned unreaped child, waits 2 seconds, sends KILL if
needed, and allows 5 seconds for waitpid confirmation. It never signals a PID after
reaping it. A poisoned channel skips Close and starts external TERM/KILL disposal;
no status/next-generation request is sent. It reports reaped exit or unconfirmed
child over launch control before host exit where possible.

Java's independent Dispose requests host disposal and allows up to 15 seconds for
the result. If the host stalls, Java calls `destroyForcibly()` on its retained host
Process, then waits up to 5 seconds for **host** exit; host death in turn causes
kernel parent-death SIGKILL of the armed child. No Java child PID/group signalling,
pidfd transfer or in-host emergency handler is assumed. An already-dead host needs
no repeated kill. Host exit is observed with Process wait/onExit; child exit is
confirmed only by a previously received exact waitpid report or separately established
exact-child exit evidence. Without that evidence report child disposal **unconfirmed**,
even when host exit and successful arm are known. FD3/control EOF is not that evidence.
No registry, process scan or PID reconstruction is added to improve the claim.

Containment scope is the Java-owned Lua-host-plus-DOSBox-child chain, not a broad
process tree. The parent-death policy applies to the one disposable DOSBox child.
R1's fixed debugger operations do not
require host subprocesses from DOSBox. The separate group prevents accidentally
signalling the Lua host during normal operation, but is not a tree-containment
guarantee: PDEATHSIG is cleared in children of a fork and does not kill arbitrary
descendants. External trusted scripting that spawns host processes needs its own
existing lifecycle; do not claim this R1 mechanism contains escaped descendants.
Other operating systems need a separately designed/qualified equivalent before
support. TCP loss requests disposal without reconnect. Native departed delivery
itself has no timer; explicit disposal may discard it.

**Single startup policy:** the DOSBox launch offer owns a fixed 120000 ms monotonic
deadline from immediately before starting the Lua host through successful trace
creation, initial stopped publication and activation. No stage resets that budget.
Override stock `getTimeoutMillis()` and `getConnectionTimeoutMillis()` (stock trace wait is
10000 ms at AbstractTraceRmiLaunchOffer:64,739; connection default is ten minutes)
with the positive remaining budget, and cancel all outstanding waits at the same
launcher deadline. A wait entered with no remaining budget fails immediately, not
with an API's zero-as-infinite convention. The Lua host receives the remaining
startup budget at its initial launch-control exchange and uses a local monotonic
deadline for connect/negotiate, setup/bootstrap and native-ready/create/publication;
Java's original deadline remains authoritative against transport delay or a hung
host. Do not compare Java nanoTime and POSIX clock origins directly.

Progress reports distinguish host launch, Trace RMI connection/negotiation, DOSBox
child creation, child arm, FD3/bootstrap, admitted native-ready, trace creation and
initial publication. Show the last confirmed stage and elapsed time, and offer
Cancel throughout without requiring a trace or Program. Timeout/cancellation reports
the pending stage, preserves earlier facts, cancels launch and runs the disposal
path above. It is a launch-policy outcome, not a guest failure or invented native
terminal. For example, native-ready timeout differs from a failed CreateTrace after
native-ready. The disposal budget is separate; startup is not silently extended
while cleanup runs. Slow supported startup may exceed ten seconds but must fit this
explicit bound; arbitrarily slow boot has no promise of success. After launch,
this startup timer is removed and cannot truncate normal terminal-only delivery.

After ready, create trace, publish initial stop and activate cpu0/frame0. Import
through stock ComLoader (explicit choice if heuristic misses), then manual Map
Module using observed load/PSP/image extent and selected static code block. Typical
relation is static `0000:0100` → native `PSP:0100`; compare appropriate image
provenance/ranges, not filename/current CS. Static PSP synthesis is labeled distinct
from captured PSP bytes. Store mapping with native load lifespan; never rebase
static Program per launch. Labels/types/comments remain static; self-modified bytes
remain trace observations and dynamic re-disassembly. Copying captured code into
a Program is explicit and records capture provenance.

Lifecycle controls:

| Control/fact | Behavior |
|---|---|
| Pause | exact Active stop request; retains usable managed session |
| Native selected-program/root departure | revoke authority, deliver known terminal/session fact; not proof of child exit |
| Target Terminate | guarded request for containment/disposal of this owned child; requested/confirmed/unconfirmed outcome, no invented native cause |
| Launcher Dispose | independently stop owned process lifetime, usable with history selected or poisoned channel |
| Connected trace Close | end live session and initiate child disposal; saved database may remain; not native settlement proof |
| Trace Save / historical browse | no guest/process effect |
| Worker Close | stop interpreter with authority revoked; not confirmation of child exit |
| EOF | channel loss/host fact; never successful Run completion |

Normal disposal is Lua-host-owned; emergency disposal kills Java's owned host
Process and relies on the child's armed Linux parent-death policy. These are not
equivalent to observing child exit. Unconfirmed child exit stays unconfirmed; no
cleanup, rollback or finalizer claim follows. No session reconnect/attach offer.

Install only into a dedicated package environment: Linux/SDL2 managed-Lua build,
matching embedded Lua ABI/modules, Ghidra 12.2/Trace RMI 12.2, 64-bit JDK 25,
Lua 5.4 external runtime with pinned luaposix and lua-protobuf codec, and exact Ghidra
12.2 proto/descriptor plus Lua message/action/schema code. Build the descriptor with
native `protoc` or the pinned Lua compiler; ship no Python-generated client and use
no Python build helper for this package. Stage 0 artifacts remain usable without Ghidra; R1 bundles the
qualified composition. Agent, MCP and AI providers are not runtime dependencies.
Record hashes/versions and preserve dependency notices including GPL-derived
DOSBox and Apache-2.0 Ghidra components; process separation is not a legal conclusion.

Trusted local launcher owns evaluator/FD3. The closed Ghidra interface is not a
sandbox against that launcher. Default networking is loopback/local only; remote
control requires an explicit protected deployment decision, not TCP reachability.
Avoid verbose paths, configuration/credential dumps and capture logging. Help
explains that intentional trace save/share can disclose guest memory.

## 11. Later delivery boundaries

R2a adds native MZ load/PSP/entry/image/relocation provenance to the same load
projection and actual Module/Section objects. Use stock MzLoader's real imported
bases; compare relocation-aware evidence rather than raw full-image equality.
Map two runtime bases to the same static Program. No auto-overlay framework,
static pointer rewriting or runtime-DS decompiler promise.

R2b adds a separate launcher choice invoking the already qualified compatible
fresh-initialized restore path, mutually exclusive with program replay. New child,
new session, new trace; inspect saved PC/registers/selected bytes stopped before
explicit execution. Archive failure yields truthful refusal/disposal, not ready.
Carry available archive/profile provenance only; no interpreter stacks/connections
or prior events restored. Optional breakpoint intent is reapplied using new exact
native registrations. No Save action is implied. Stage 0's restored-root proof
does not commission this UI.

Later DOS lifetimes add only evidenced load/residency/overlay transitions to the
native load owner and mapping lifespans; child exit is not universal guest death.
Protected/mixed profiles require actual consumer decoding/navigation evidence
before selecting a fixed-language representation. Native loaded cache queries,
arbitrary-selector/current-table queries, page walks and physical backing reads
remain distinct. No Lua/Java MMU, new language/fork or per-selector registry is
designed now. HMA requires both native A20 qualification and the documented 12.2
pre-dispatch rounded-range boundary remedy/proof. These are extension points in
observations/capabilities, not currently advertised operations.

## 12. Failure rules and invariants

| Failure | Required result and allowed next action |
|---|---|
| Known pre-admission unsupported/stale/Pending refusal | complete typed no-effect response; later fresh deliberate request permitted |
| Complete inspection failure | no bytes/effect; mark range unavailable/error; later fresh inspection allowed |
| Complete Pending/Active Wait | unresolved exact operation; later Observe/Wait/Pause as applicable; no duplicate Start |
| Known effect then reporting/trace failure | retain known effect plus missing projection; fresh stopped reads only, never replay |
| Truncated/malformed frame, generation mismatch, transport timeout/unknown completion | poison channel even for reads; no next record, Observe, Close, resend or reconnect; external disposal |
| Copy/terminal publication failure at native departure | fail closed with known facts retained if possible; do not invent terminal or block teardown on remote receipt |
| Expired/invalid callback verdict/read | no provisional authority; explicit error/stop; never resume from stale verdict |
| Unsupported stopped mode | raw available observations, no misleading addresses/decoding/effects; safe control and disposal |
| Host death without terminal receipt | host outcome known if confirmed; native cause/outcome unknown unless already received |
| Release response lost | no retry; slot consumption may be known only internally; dispose externally |
| Ghidra guard missing/version mismatch | launch refusal, no stock fallback |
| Disposal failure | expose unconfirmed child identity/ownership status; never report success |

Invariants: one attachment, one operation, one evaluator and exchange; no guest
effect from historical dispatch; no ordinary read while Pending/Active/event;
no provisional read after native hit lifetime; no terminal delivery dependency on
departed native frames; exactly one ordered release representation consumed; no
result resurrection; no generation recovery; no trace transaction pretending to
roll back a guest effect; no unqualified decoding/translation advertised as native
truth. These are ordinary correctness boundaries, not root-process containment.

## 13. Staging and targeted technical validation

Implementation plan must name changed source owners/callers, docs and exact package
artifacts, and refresh source pins before mutation. Recommended coherent order:

1. **Stage 0 core/protocol proof:** shared Active service, split handoff, exact
   identity/result/release, host/client v1 and non-destructive wake. Run A0/A1 with
   Agent disabled, condition-free loop, Pending refusal and both departure orderings.
2. **Stage 0 complete migration:** synchronous callback/event lifecycle and affected
   `dbg.live` callers on same core; callback pending close/expiry and ordinary
   breakpoint/manual continuation; unrelated input/capture regression. A0b artifact
   checks include docs, clients, package metadata/notices and unchanged dependency.
3. **Early R1 Lua carrier/consumer proof:** first exercise the actual Lua host with
   12.2 negotiation/method declarations, XML root, transaction, snapshot, exact
   integer/register/memory encoding and a reverse invocation. Test partial frames,
   FIFO reverse replies with Interrupt priority, oversize/error and native disposal
   while TCP is backpressured. Then isolated patched 12.2, real managed COM, actual typed
   windows/frame/register overlay, nonaligned CS `1234:0100`, AX `0200`, `JMP AX`
   native `0x12540`, near-relative boundary and far transfer. Decide justified UI
   restriction/remedy with actual evidence before expanding adapter work.
4. **R1 coherent product:** mapping, breakpoints/hit identity, freshness/read splitting,
   history guard all routes, child containment and installation/help. Run A2–A5
   through the actual combined package. No synthetic trace-only acceptance.
5. R2a/A6 and R2b/A7 independently. Later increments: qualified profiles/A8 independently.

Use a few discriminating native/protocol race fixtures plus actual consumer paths:
hold Pending without activation; queue stale Pause against successor; release wins
versus departure wins; no-operation departure; idle read wake with partial frame;
wait returns Active just before root exit; callback hit remains held for reads;
late verdict/close; truncate one effect reply and prove no next generation.
No exhaustive scheduler interleaving/device matrix is required.

Pending barriers provide a test opportunity, not a promise that transported Observe
will see Pending: Pending/Active/Terminal are valid according to consumption order.
Assert native Pending Pause refusal/no forced activation and truthful UI state.
For Linux lifecycle, kill the host before arm (including subreaper reparenting),
after arm/before report, after exec and while stalled; exercise Java forced host
termination, live-host TERM ignored then KILL, failed setup and delayed/unconfirmed
exit. Assert no DOSBox exec/readiness after failed parent check, no cleanup guarantee,
and no child-confirmed claim from host exit alone. Keep independent test-owned child
exit evidence separate from the product's deliberately unconfirmed emergency report.
Test ordinary exec preserves the signal and reject unsupported credential-changing
launches; do not claim containment of arbitrary subprocess descendants.

A2 startup tests exceed stock ten seconds but finish within the 120000 ms deadline,
then exercise expiry/cancellation during native-ready wait and during create/initial
publication after ready. Check the last confirmed stage, no deadline reset, actual
offer wait overrides, responsive cancellation and separate disposal outcome. Include
CreateTrace oid/open-use/close/non-reuse and unknown raw reason versus invalid
terminal schema/enum tests in the Lua carrier proof.

R1 A3 must prove unknown memory at the new stop in actual Listing/time, full 4-KiB
subdivision with holes and a representative no-side-effect sentinel; logical-wrap
refusal contrasted with linear read. A4 must exercise historical toolbar, shortcut,
context/breakpoint/Terminate and queued selection-change paths, including away/back.
A5 records one in-flight exchange followed by Interrupt ahead of refresh demand,
local endpoint defaults, argument quoting and trace-publication failure. Preserve
failures and exact build/profile/package identities; no performance claim until measured.

Documentation owners: `docs/managed-lua-session.md` for v1/library/lifecycle contract;
current Lua/live debugger API docs for singular async/sync/callback behavior;
integration install/help for guard distribution, closed actions, profile/memory/time
limits, disposal and data handling; package manifests/notices; test/reproducer index
for exact acceptance receipts. Do not rewrite these approved analysis/spec documents as implementation
evidence. Implementation plan is a subsequent artifact, not this document's edits.

## 14. Complete requirement ownership / verification traceability

Each row names a mechanism above and the functional specification's acceptance
scenario. Later scope remains later; shared scope/claim review covers exclusions.

| Requirement | Owner / design mechanism | Verification |
|---|---|---|
| GI-SCOPE-01 | §§2,6.4,8,10 external Lua negotiation/XML/actions over native guest | A2 |
| GI-SCOPE-02 | §§11,13 independent packaged deliveries | scope review, A0b/A2/A6/A7 |
| GI-FLOW-01 | §10 stopped COM launch/manual map; §8 actions | A2 |
| GI-FLOW-02 | §8 snap-keyed observations; §9 guard | A2/A3/A4 |
| GI-FLOW-03 | §11 new restore launch/trace | A7 |
| GI-LIFE-01 | §§2–4 existing exclusive native owner | A0/A5 |
| GI-LIFE-02 | §§2.1,4,6.3 external Lua host/sole embedded dispatcher, singular core/wrappers | A0/A0b |
| GI-LIFE-03 | §3 ready identity and state separation; §8 cpu0 | A0/A2 |
| GI-LIFE-04 | §§2.1,3,5,10 Java-owned host Process + child pre-exec PDEATHSIG/parent check; separate exit facts/startup policy | A1/A2/A4/A5 |
| GI-OP-01 | §§3–4 prompt native admission | A0 |
| GI-OP-02 | §§3,6 fixed nondestructive exact observation | A0/A1 |
| GI-OP-03 | §3 consumption-time Pause table | A0 |
| GI-OP-04 | §4 condition-free shared Active service | A0/A5 |
| GI-OP-05 | §§3,5 retained terminal/release | A0/A1 |
| GI-OP-06 | §8.3 native/raw taxonomy | A0/A2 |
| GI-OP-07 | §4.3–4.4 synchronous core + held event | A0 |
| GI-TERM-01 | §5 native serialized transfer/release | A1 |
| GI-TERM-02 | §5 same-FD wake/top-level delivery owner | A1 |
| GI-TERM-03 | §§5–6 fixed dispatch/live refusal | A1 |
| GI-TERM-04 | §5 response flush/worker close/join | A1 |
| GI-UI-01 | §8.1 actual interfaces/register/frame schema | A2 |
| GI-UI-02 | §8.3 closed actions; §10 lifecycle | A2/A4 |
| GI-UI-03 | §§3,7,8 native capability projection/admission | A2/A5 |
| GI-BP-01 | §7 exact address and registration/native IDs | A2 |
| GI-BP-02 | §7 native hit producer/internal attribution | A2 |
| GI-OBS-01 | §§7–8 exact revision and serialized publication | A0/A3 |
| GI-TIME-01 | §9 all-path versioned consumer guard | A4 |
| GI-MEM-01 | §7 native safe linear inspector | A3 |
| GI-MEM-02 | §7 bounded safe subdivision/holes | A3 |
| GI-MEM-03 | §8.2 explicit unknown state and incomplete batches | A3 |
| GI-ADDR-01 | §7 native ordinary-cache admission | A2/A3 |
| GI-ADDR-02 | §§7–8 real-mode coordinates/wrap refusal | A2/A3 |
| GI-ADDR-03 | §9 restricted semantics + consumer proof gate | A2 |
| GI-MAP-01 | §10 actual COM load and manual stock mapping | A2 |
| GI-MAP-02 | §§8,10 mapping lifespan/static knowledge separation | A2/A6 |
| GI-MAP-03 | §11 MZ native provenance/stock sections | A6 |
| GI-ADV-01 | §11 later native load-lifetime observations | A8 |
| GI-ADV-02 | §11 native translation/profile consumer gate | A8 |
| GI-REST-01 | §11 fresh process/trace, stopped before Run | A7; bounded A1 precursor |
| GI-REST-02 | §11 archive/interpreter separation and refusal | A7 |
| GI-FAIL-01 | §§6,8,12 effect honesty/no replay | A5 |
| GI-FAIL-02 | §§6,10,12 poisoned FD3; Lua exact child disposal or Java host kill → kernel parent-death containment; unconfirmed exit honest | A1/A5 |
| GI-PKG-01 | §§2.1,6.4,9–10 pinned Lua protobuf/POSIX/descriptor and guarded 12.2 | A2 |
| GI-PKG-02 | §6 host/client v1 and immutable dependency | A0b |
| GI-TRUST-01 | §§6.3–6.4 closed Method table → typed managed records → fixed Lua binding, native identity | A0b/A5 |
| GI-TRUST-02 | §10 local endpoints/argv/data/notices | A5 |
| GI-PKG-03 | §§6.2,6.4 bounded one-in-flight scheduler/FIFO/Interrupt priority; §8 publication model | A3/A5 |
| GI-LIMIT-01 | §§1,11–13 scoped architecture/non-goals | scope/claim review |

## 15. Alternatives and design-value reassessment

Rejected: direct RPC/GDB fallback (violates selected carrier); Python thread around
blocking Run (still occupies sole evaluator/channel); repeated bounded stepping
(changes continuous Run); Agent-only pump (fails selected build); two execution
cores to preserve old callbacks (unmaintainable authority split); copied provisional
tokens after hit unwind (dangling authority); retaining ExitFence for remote release
(deadlocks native teardown/lifetime); shutdown wake on departure (destroys delivery);
result catalog/reconnect/retry (unnecessary and violates uncertainty contract).

Rejected: launcher-only UI guard (direct built-in sends bypass it), backend-only
historical check (no selected time), rewriting EIP to flat PC (false register data),
PC tracker as SLEIGH fix, Java/Lua MMU, per-segment address spaces, whole RAM dumps,
automatic full-image equality for MZ, permanent static rebase, invented OS threads,
automatic detach/attach recovery and installation-time patching of user Ghidra.

Design-value reassessment: the carrier is more work than RPC, intentionally. The
valuable reusable result is one managed async core over established startup/restore
and native ownership, with synchronous scripting preserved. The costly pieces are
not optional ceremony: Active service, provisional hit lifetime, departure copy and
consumer effect guard close specific demonstrated gaps. Bound complexity to one
operation, one event, one delivery copy and one client; use existing native identity
and trace history rather than registries. R1 is useful without MZ, restore UI or
advanced CPU representation. No speculative generalization is necessary now.

Lua-only delivery adds real work: a new version-pinned Trace RMI message client,
XML/schema/action declarations, dual-direction FIFO scheduling, exact integer and
register serialization, installed codec/POSIX ABI and child-lifecycle qualification.
These do not come from the dependency JSON codec or its old supervisor. Separately
price the native async/callback/terminal refactor, the small managed union/client,
the Lua presentation/protocol package, and Java consumer guard/launch containment.
Containment specifically costs a small Linux pre-exec arm/expected-parent/setsid
binding and setup report, Java direct Process ownership/disposal and stream draining,
Lua exact waitpid/escalation, and qualification of their races. It does not require
pidfd/SCM_RIGHTS plumbing, a resident guardian or process-tree registry. The launch
offer additionally owns one deadline, stock wait overrides, staged progress/cancel
and truthful cleanup outcome. Portability is not free: other OS equivalence is
separate work before support, not an unimplemented fallback inside R1.
No unmeasured line-count or calendar estimate is claimed. The codec binding removes
handwritten protobuf machinery; the external placement keeps presentation churn
out of native lifetime control. Early carrier and native proofs are independent
stop/go checkpoints before full R1 work, not reasons to drop existing behavior.

### Remaining proof obligations, not unspecified functional ownership

The independently approved design fixes who implements every requirement. Staged
implementation and qualification must still prove Active seam safety/cost, callback deadline/close
arbitration, native load/mode/hit projection, guard coverage and GUI serialization,
register aliases, freshness and nonaligned-CS consumer limitations. Exact source
file splits and build/install script choices may change without changing the
contract; changing the fixed event deadline requires an explicit contract update.
A required consumer remedy discovered by
the early experiment must be reviewed before shipment. None of these unexecuted
proofs is claimed passed by this approved design.
