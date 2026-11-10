# Ghidra integration — functional specification

**Date:** 2026-11-10
**Revision:** 4 (Lua-only revision 2)
**Status:** Independently approved Lua-only revision 2 (Sol, GLM 5.3 and DeepSeek). Not implemented or runtime-qualified.

## 1. Authority and purpose

This specification defines the intended product behavior of DOSBox-Y as a Ghidra
Debugger backend. Its current companion authority is the revised
[Ghidra integration analysis](GHIDRA-INTEGRATION-ANALYSIS.md), incorporating the
owner's material correction: Lua end to end on the integration side, no Python
runtime component. In the `original-savestate-restart` repository/worktree,
analysis `d8710d20e` and specification `d2ac6b736` preserve prior
reviewed behavior in history but their Python-specific clauses are superseded.
This revision retains all 48 requirement IDs and non-Python races/stages. Sol,
GLM 5.3 and DeepSeek independently approved the Lua-only revision, including its
actor/carrier, packaging and typed boundaries. Approval does not prove implementation
or consumer behavior.

Requirements below use **shall** for mandatory behavior within their stated
delivery scope. Requirement identifiers are stable and unique. Later-delivery
requirements do not gate R1. Acceptance scenarios are observable release gates,
not an implementation plan or an exhaustive test inventory. Exact classes,
queues, schema field layouts, extension hooks and module factoring belong in
technical design.

**GI-SCOPE-01 — Product objective.** The operator shall be able to correlate an
imported DOS program with a controlled DOSBox-Y guest, inspect stopped state,
set supported breakpoints, execute through native debugger operations and retain
observational history in Ghidra. This debugs the DOS guest, not the host C++ process.

### Delivery boundaries

| Delivery | Functional commitment | Explicit boundary |
|---|---|---|
| Stage 0 | Shared asynchronous managed-Lua contract, synchronous convenience, terminal delivery, client/lifecycle and coherent protocol/package artifacts | May ship as an internal/shared capability; not a usable Ghidra product by itself |
| R1 | Installable useful COM debugger: managed stopped launch, ordinary real-mode RAM below 1 MiB, registers/frame-zero PC, SS:SP navigation, manual verified mapping, execute breakpoints, Run/Pause/Step Into/Over, history protection and disposal | No existing-instance attach, MZ automation, restore UX, target writes, watchpoints, unwinding, HMA/unreal/V86/protected mode |
| R2a | Real-mode MZ load/section/relocation provenance and reusable static mapping | Independent of R2b; no general loader tracing, arbitrary overlays or extenders |
| R2b | Compatible archive → fresh stopped process and new trace → inspection → explicit continuation | Independent of R2a; no in-place restore, program replay or interpreter restoration |
| Later increments | Individually qualified DOS lifetimes and protected16/protected32/mixed-mode profiles | No universal DOS/device/extender coverage promise |

**GI-SCOPE-02 — Independent useful deliveries.** Each advertised delivery shall
include its installation/use documentation, supported-profile limits and actual
consumer acceptance. Stage 0's small fresh-restored-root proof does not make
Ghidra restore UX an R1 requirement. Later capabilities shall not delay a correct,
truthfully limited R1.

## 2. Actors, artifacts and workflows

| Actor/artifact | Responsibility or meaning |
|---|---|
| Operator | Selects executable/configuration, imported Program, live actions, mapping and intentional trace saving/sharing |
| Ghidra Debugger | Displays trace observations, static analysis and time selection; guards effect invocation against historical selection |
| Trusted external Lua integration host | Owns disposable child/session lifecycle, sole managed client, Lua Trace RMI schema/actions/serialization and observation publication; one local event loop |
| Managed Lua session/native debugger | Sole selected frontend and native execution authority; admits operations, observations and instrumentation |
| Static Program | Reusable original/imported analysis, labels, comments and types |
| Trace/snapshot | Time-indexed captured observations, potentially incomplete; not a machine save |
| Native save archive | Machine state accepted by the existing compatible fresh-target restore contract; not interpreter or debugger state |

**GI-FLOW-01 — COM workflow (R1).** The operator shall be able to launch a
selected COM stopped at admitted entry, select/import its static Program, manually
map the observed image, set a static-listing execute breakpoint, continue to its
native hit, step into/over, inspect changed data, interrupt a continuous loop and
end the disposable target. Launch shall complete without requiring an imported
Program or successful automatic mapping first.

**GI-FLOW-02 — Observe and compare (R1).** Browsing listings, expanding objects,
refreshing stopped observations and selecting earlier snapshots shall not execute
the guest or modify its state. Separate launches may be compared through retained
observations and the same static analysis; historical selection is never restore.

**GI-FLOW-03 — Restore workflow (R2b).** The operator shall select a supported
archive/profile, obtain a new stopped disposable process and new trace, inspect
saved state, and continue only through an explicit execution action.

## 3. Authority and lifecycle

**GI-LIFE-01 — Exclusive ownership (Stage 0 onward).** There shall be one native
debugger attachment and one current execution operation. Ghidra shall use the
managed Lua frontend, not attach as a second observer beside another frontend.
A competing native attachment, including observer-only attachment, shall be
refused until full detach and cleanup. UI focus never transfers control.

**GI-LIFE-02 — Selected carrier (Stage 0 onward).** The integration shall use an
external Lua Trace RMI integration host over asynchronous managed Lua and the existing
native owner. The host shall own lifecycle and presentation; the sole embedded Lua
worker shall own debugger binding dispatch. There shall be no Python package,
process, adapter, generated client or runtime dependency. A small native codec/OS
binding may support Lua but shall not own debugger schema, actions or projection.
Synchronous helpers and affected existing Lua execution callers,
including relevant live execution entry points, shall use the same core.
Direct Agent RPC is neither a production carrier nor a fallback. Unrelated
supported frontend/input/capture behavior shall be preserved. Difficulty in the
selected carrier requires redesign of its seams or escalation of a concrete
impossibility, not an unapproved carrier switch.

**GI-LIFE-03 — Startup and state separation (Stage 0 identity; R1 UI presentation).** Process creation, transport
connection, native stopped admission, operation admission, activation, settlement
and host exit shall remain distinct facts. Initial registers/PC/bytes shall be
published only after actual stopped native admission. The UI shall not claim
RUNNING merely because a request was accepted, or STOPPED merely because Pause
was requested. One guest CPU execution thread shall be represented; host worker,
audio and renderer threads shall not become guest threads. DOS activation/PSP
metadata shall not imply independently schedulable host-like processes. The managed
session shall return a stable session identity at creation/readiness, before any
operation is required. Relevant admission, observation and result responses shall
identify that session; session-only departure shall be addressable using it.
Identity shall derive from the native/session lifetime, not a new registry or
generation-recovery protocol. This identity obligation applies from Stage 0;
Ghidra state/thread presentation applies from R1.

**GI-LIFE-04 — End-of-session policy (Stage 0 onward; Ghidra UI clauses R1).** The launcher shall own a disposable
emulator child. Connection loss shall end that live debugging session and trigger
containment/disposal through supported native/process lifetimes. Pause preserves
the usable session. Native target exit, native attachment departure, worker close, trace close, disposal
request and confirmed process disposal shall be reported separately. Closing or
saving the trace shall not itself be taken as proof of native settlement or child
exit. Failed disposal shall identify an unconfirmed child. R1 shall offer guarded
target Terminate (request containment/disposal of its owned child), independent
launcher-owned Dispose, and trace Close. Closing a connected trace ends its live
session and initiates owned-child disposal; retaining/saving the database is
separate. Terminate/Dispose report requested versus confirmed/unconfirmed disposal,
not an invented native exit cause. R1 shall register no Detach action and shall
not offer nondestructive detach leaving a child running, whether newly launched
or pre-existing. Native attachment departure remains a reportable fact, not a
separate UI capability. These controls obey GI-TIME-01 and GI-TERM-01–04 where
applicable; channel ambiguity requires external disposal under GI-FAIL-02.

For Linux R1, Java shall own the external Lua host's directly launched Process.
The Lua host shall create the disposable DOSBox child through a small pre-exec
lifecycle binding that arms `PR_SET_PDEATHSIG(SIGKILL)` and checks the exact expected
parent captured before fork, before exec/readiness. The host shall report successful
arm before native-ready. Parent death before arm must cause fail-closed setup on the
parent check; death after arm uses kernel SIGKILL. Graceful disposal/reaping is
Lua-owned. A stalled host shall be terminated through Java's retained Process,
triggering the child's armed policy, without relying on a child handle transfer or
code in the dead host. Host exit, child-armed and child exit are separate facts;
without exact child-exit evidence disposal stays unconfirmed. SIGKILL makes no
cleanup/finalizer/result-delivery guarantee or bounded exit promise. Containment
covers the Java-owned Lua-host-plus-DOSBox-child chain, not an arbitrary process
tree; the parent-death policy applies to the disposable DOSBox child. Qualified execution shall
preserve the parent-thread/ordinary-exec/credential conditions of the Linux policy;
other OS support requires a separately designed and qualified equivalent.

R1 launch shall use one explicit bounded startup policy: 120000 ms from host launch
through initial stopped trace publication/activation, shared by launcher/host without
stage resets. The offer shall replace stock trace/connection wait budgets with the
remaining deadline, report progress and allow cancellation before a trace exists.
Timeout shall identify the pending stage (host launch, child creation/arm,
FD3/bootstrap, native-ready, Trace RMI connection/negotiation/create/publication),
retain prior known facts and dispose through the selected lifecycle. Timeout is not
guest failure. Cleanup uses its separate bound; no indefinite startup is promised.

## 4. Asynchronous execution and results

The following names describe behavior, not frozen function signatures.

**GI-OP-01 — Start (Stage 0).** Start shall promptly admit one supported native
Run/Step/Over/run-until operation against the exact session and stopped context,
returning its stable native operation identity, session identity and Pending state
or a known refusal. Fast
completion may return Terminal. Admission shall not wait for guest completion;
Pending does not prove activation, and Active does not prove instruction retirement.

**GI-OP-02 — Observe/wait (Stage 0).** Observe shall nondestructively report the
exact operation as Pending, Active, event-pending where applicable, or Terminal.
Wait shall bound native waiting and return the same operation's state; expiry
means unresolved, not stopped, cancelled or implicitly paused. Neither operation
shall invoke arbitrary Lua callbacks. Stale, unknown, released or retired operation
identities shall be explicit refusals on an otherwise healthy open session; their
results shall not remain indefinitely observable after release. A departed result
remains observable by exact identity only until its delivery-slot release/close;
after closure the channel is unavailable, not a result lookup service.
Bounds are not hard real-time guarantees over foreign code or broken transport.

**GI-OP-03 — Pause (Stage 0).** Pause shall be checked by the native consumer
against the exact attachment/session and operation when consumed. Only an Active
operation may accept it. Pending shall be refused without effect or forced
activation; there is no nondestructive Pending cancellation primitive. A later
fresh Pause may succeed after normal activation. A retained held Terminal shall
return its terminal fact; stale/unknown/released identities shall be refused.
After departure Pause shall be refused in favor of terminal observation. A delayed
Pause shall never stop a successor operation. Acceptance is not settlement and
shall not mean kill, worker close or process disposal.

**GI-OP-04 — Native execution semantics (Stage 0/R1).** Continue shall preserve
unrestricted Run without an invented ceiling. Step Into/Over shall use native
operations, not integration-host instruction loops or disassembly-derived stepping.
Condition-free Active Run shall remain observable and pausable through the same
sequential managed channel in a managed-Lua build with Agent/RPC disabled.
No additional stop condition, second CPU loop or repeated synthetic phases may
substitute for this behavior. Control may wait behind one in-flight short exchange,
but not a queued stopped-refresh backlog or an evaluator occupied for Run's duration.

**GI-OP-05 — Result identity and retention (Stage 0).** One settlement shall carry
session/operation identity, outcome (stopped, exited or failed), raw native reason,
available end-state/revision identity, register validity and continuation
availability. Context/revision authority shall derive from native lifetime and
checkpoint facts, not Lua-host counters. Invalid contexts shall not be fabricated.
At an ordinary held stop, repeated observation shall preserve the result; exact
release shall retire it before another execution occupies the slot. There shall
be no unbounded result catalog or replay protocol. Breakpoint/manual stops shall
permit later execution when native ownership permits, not latch a false error.

**GI-OP-06 — Stop taxonomy (Stage 0/R1).** Results and UI shall distinguish
startup/entry, step completion, user breakpoint, requested Pause/manual stop,
explicit condition match, budget exhaustion, cancellation, program/root exit,
fault/error and owner/attachment/continuation loss. Budget shall retain its actual
unit/meaning. New or unmapped reasons in structurally valid terminals shall be
`unclassified` with the raw value; continuation depends only on its explicit native
field. Invalid protocol enums or terminal schemas fail rather than being normalized.
A wait timeout is not a stop reason. Host exit/disposal is not a native cause;
where causality is unknown, report target ended with cause unknown rather than
inventing natural or requested exit.

**GI-OP-07 — Synchronous convenience and callback preservation (Stage 0).**
Synchronous helpers shall perform start, bounded waiting/event service, terminal
copy and release on the same core, intentionally blocking only their scripting
caller. Ghidra shall use async primitives. Existing supported callback behavior
shall be migrated and proven before claiming its preservation or replacement;
removal needs a scoped owner decision. At most one exact provisional event may
exist, bound to operation, registration and native lifetime. Native provisional
event authority shall hold guest execution/state at the actual hit until the exact
worker verdict is consumed or error, expiry, close or departure settles the event.
Provisional reads shall use that still-valid held native hit/continuation context,
never post-resume state. A copied image/token shall not extend a native stack
borrow or permit execution to resume while its event remains pending. Only the sole Lua
worker's explicit event service may invoke its callback; no native-thread Lua,
concurrent evaluator, recursive execution or guest effects from predicates.
Provisional read authority is not ordinary stopped authority and expires on
verdict/error/close/departure before its native lifetime disappears. Unserved
events shall settle as explicit error/stop rather than strand ownership. Callback
budgets do not guarantee preemption of blocking foreign code; external disposal
remains available. R1 shall explicitly refuse execution with armed callback-bearing
registrations, never silently discard them.

## 5. Native departure and terminal-only delivery

**GI-TERM-01 — Immediate authority revocation (Stage 0).** Native departure shall
immediately revoke guest effects and stopped/provisional contexts. Before native
result storage is retired, the session shall retain a copy of its one terminal
fact with exact identity, available native exit/revision facts and
`continuation=false`. Missing facts shall be unavailable, not synthesized. With
no selected operation, only a session-departure fact shall be offered; no operation
identity or release receipt shall be invented. Unknown fatal teardown remains unknown.

Departure publication, exact release consumption and session dispatch decisions
shall be serialized with one ordered winner; no cross-system transaction is
implied. If exact release consumes an ordinary held result first, it commits one
ordinary-release response and frees that slot. A subsequent departure with no
new operation offers only the session-departure fact; it shall not recreate the
released result or replace/duplicate that release response. If departure transfer
wins first, exact release consumes only the copied terminal-delivery slot, answers
once and follows departed-session closure. An in-flight response already decided
shall not be discarded or rewritten by departure; native teardown shall not wait
for remote receipt. Response delivery may still fail under GI-FAIL-02. Only a
complete matching exchange proves receipt; internal consumption alone does not.
A wait racing departure may return Active or Terminal according to its native
observation point; subsequent permitted observation sees the retained terminal
fact. No outcome authorizes double consumption, another settlement or replay.

**GI-TERM-02 — Delivery window (Stage 0).** The same child, sole worker and managed
channel shall remain available solely for copied terminal delivery after native
departure. Native cleanup/unwinding shall not wait for remote release inside a
borrowed guest frame, retain a live CPU frame for answering requests, or wait for
a worker needing a departed native seam. Idle readers and bounded waiters shall
become aware of departure without closing the transport or injecting unsolicited
response bytes. No reconnect, new child, attachment resurrection or re-admission
shall continue the old session.

**GI-TERM-03 — Permitted requests (Stage 0).** Fixed non-evaluating observe/wait,
release and close shall work on the healthy channel both before and after
departure, without requiring the client to guess that transition. Terminal wait
shall return immediately and repeated observation shall return the same facts.
After departure all new Run/Step/Pause/read/registration/callback/evaluator
requests shall be refused before evaluation/native dispatch. A racing live execute
shall receive a complete refusal; only after that exchange completes unambiguously
may fixed observation follow. An already-running synchronous helper may finish
consuming its terminal and flush its response without regaining guest authority.

**GI-TERM-04 — Closure (Stage 0).** For a departed operation, exact release shall
consume the delivery copy and flush its response before normal worker/session
closure; mismatched identity shall not consume it or call the retired native
owner. Explicit close may discard the copy with acknowledged loss. Session-only
departure uses observation and close, not fabricated operation release. Worker
closure shall precede final join/owned-resource closure and ordinary child exit;
Lua finalizers shall not regain guest authority. Peer loss/external disposal may
lose delivery without receipt. There is no automatic delivery timeout, durable
journal or guaranteed delivery. Native outcome remains unknown if not received;
host EOF is never successful Run completion.

## 6. Ghidra-visible observations and actions

**GI-UI-01 — Useful standard views (R1).** Trace RMI shall populate the actual
Registers, Dynamic Listing, frame-zero Stack/PC, Memory Map, Modules and
Breakpoints consumers through compatible typed objects and actions, not merely
descriptive Model properties. Initial publication shall activate the actual guest
thread/frame so those consumers show the admitted stopped target. R1 shall expose
the COM module/ranges needed for its manual mapping; section objects are included
only where that mapping actually uses them. MZ section/load/relocation behavior
is R2a, not an R1 prerequisite. Register values/names/widths shall match the selected
language, with no silent loss of required registers. The top frame shall carry
the effective current-instruction address while CPU IP/EIP remains its true
logical register value. Normal PC tracking shall follow far transfers; explicit
effective SS:SP navigation shall be available. No full call stack, locals, symbols
or call-return history shall be fabricated. FPU/control registers are advertised
only when actually exported and qualified.

**GI-UI-02 — Closed supported action surface (R1).** The supported surface shall
provide stopped register/environment refresh, safe memory reads, Continue,
Interrupt/Pause, native Step Into/Over, native execute-breakpoint creation/removal
and the R1 lifecycle controls enumerated in GI-LIFE-04: guarded target Terminate,
independent launcher Dispose, and trace Close; no Detach action. Focus activation shall be read-only and carry
no time-travel effect. Step Out, reverse operations, hardware/read/write/access
watchpoints, conditions, toggles and one-shot actions shall be absent unless their
exact selected Lua/native capability has separately been delivered. R1 shall
register no target memory/register editing, free-form command, console forwarding
or general execute action. A trace database edit is not a guest edit.

**GI-UI-03 — Capability honesty (all deliveries).** Advertised actions shall
derive from actual selected managed-Lua operations and native build/core/context
capabilities, exposed as session/status facts and rechecked at use. A method in
RPC or a diagnostic interface is not a Lua capability. Unsupported actions shall
not be inert or false-success buttons. No new capability registry is required.
Any later target edits shall be stopped-only native effects with readback and
separate effect/reporting outcomes; a failed readback shall not imply no write.

**GI-BP-01 — Address and registration truth (R1).** A static breakpoint click
shall use the selected mapping and qualified native address conversion. Ordinary
real-mode aliases shall identify the same advertised execution location, not an
invented persistent CS-selector predicate. The trace shall retain exact
session-scoped Lua registration identity, native breakpoint identity, owner/kind,
temporary/internal status, available hit count and raw reason from the actual hit.
Deletion shall expire its trace location. Unsupported toggling shall be absent,
not silently implemented as delete/recreate. MemoryChange is not a read/access
watchpoint.

**GI-BP-02 — Hit attribution (R1).** The actual hit shall identify the applicable
user registration among multiple armed breakpoints. Internal step-over/run-until
breakpoints shall report internal completion/condition, not a guessed user hit
based on PC equality. Unavailable attribution shall be explicit. Later watchpoint
support shall preserve the same identity and result rules.

## 7. Stopped coherence, time and memory

**GI-OBS-01 — Exact stopped observation (Stage 0/R1).** Native reads and
registration/removal shall validate the exact current owner and stopped context
at use. Async control does not permit ordinary guest inspection while Active.
Each published observation batch shall belong to one native session/stop revision;
effects and batches shall be serialized and returned identities checked. Stale
data shall not be combined with, or retargeted to, a later stop. Refresh while
history is selected shall enrich only its corresponding still-current live-stop
snapshot, with explicit publication time/lifespan, never the GUI's historical cursor.

**GI-TIME-01 — Historical-view no-effect guarantee (R1).** A version-matched
Ghidra-side invocation guard shall compare actual selected trace/time, published
live trace/time, live target identity and applicable state. It shall disable and
recheck effect dispatch for toolbar, keybinding, context-menu, breakpoint, edit
and terminate paths. A selection change before dispatch invalidates a queued
action. An effect already validly dispatched while live is not retrospectively
cancelled by browsing. No usable unguarded standard effect action may ship.
Backend current-context checks remain independently mandatory and cannot substitute
for the selection guard. Launcher-owned disposal remains available independently.
This protects ordinary operator selection, not hostile direct local method calls.

**GI-MEM-01 — Safe and explicit meaning (R1).** Trace `ram` shall have the stable
meaning of the qualified linear observation view. Logical segment:offset,
linear and bus-physical coordinates shall remain distinct. Reads shall not run
instructions, load segment registers, acknowledge devices, invoke DOS services,
raise guest faults or alter accessed/dirty bits to obtain data. Unreadable/MMIO
ranges shall remain unavailable rather than zero-filled or accessed through a
side-effectful fallback. Debugger inspectability is not proof of guest permission
or CPU-effective translation.

**GI-MEM-02 — Bounded complete reads (R1).** The Lua integration host shall handle Ghidra's
automatic 4096-byte requests by bounded native reads respecting actual limits
and known safe boundaries, including complete subdivision if a native limit is
smaller. Only successful subranges may publish bytes; holes/failures shall remain
explicit. A failed response shall contribute no bytes. Without safe subdivision
information a whole block may be unavailable; no byte-by-byte discovery scan or
device-read fallback is permitted. A shortened response shall not masquerade as
a complete block.

**GI-MEM-03 — Freshness (R1).** At a new stop, previously observed memory shall
not inherit current-known status without fresh observation. Historical bytes shall
remain at their original time; unrequested current ranges shall be unknown, not
error. Successful current reads become known and actual failed reads unavailable/
error as appropriate. Actual Listing/time behavior shall demonstrate this property;
incrementing a snapshot number alone is insufficient. Publication failures shall
leave observations explicitly incomplete, not claim an atomic guest/trace update.

## 8. Segmentation and analysis correlation

**GI-ADDR-01 — Ordinary-real-mode admission (R1).** Address-sensitive behavior
shall require native evidence of ordinary real-mode interpretation, including
the relevant hidden-cache validity/base/limit/default-size facts or native
admission asserting them. A coarse `cpu_mode` or `!pmode` alone is insufficient.
Qualified memory is ordinary real-mode RAM below 1 MiB. HMA, unreal, V86 and
protected contexts shall not be inferred supported. At an unsupported stopped
context retain available raw observations and safe interrupt/disposal, but disable
misleading decoding, mapping and address-sensitive effects. Continuous Run need
not stop at every transient mode transition; no such monitoring is promised.

**GI-ADDR-02 — Alias preservation (R1).** The trace shall use the selected Ghidra
real-mode language/compiler (`x86:LE:16:Real Mode`, `default`) while preserving
actual segment registers and requested logical coordinates separately from flat
wire addresses. Normalized Ghidra spelling shall not be treated as original CPU
coordinates. No global 20-bit mask shall simulate A20. Explicit segment-relative
requests shall define offset wrapping separately from linearly advancing reads;
the two shall not be silently substituted across offset `ffff`.

**GI-ADDR-03 — Truthful analysis limits (R1).** Correct bytes, instruction display,
frame PC and native stepping shall be demonstrated independently of dynamic
control-flow references and p-code semantics. Near-indirect, near-relative and far
transfers at nonaligned CS shall be checked against native destinations. A PC
tracker alone shall not be claimed to repair segmented language semantics. R1
may ship affected dynamic reference/p-code/evaluated-target features explicitly
unqualified and not offered as reliable navigation; if normal UI cannot make the
limitation clear enough, a narrow consumer remedy is required before that workflow
ships. Universal segmented decompilation is not an R1 gate. Static mappings and
changed DS properties shall not imply corrected decompiled pointers or hardware-
accurate Watches/p-code emulation.

**GI-MAP-01 — COM mapping (R1).** Reuse Ghidra's COM loader, with explicit selection
where its heuristic needs help. Manual Map Module/Section shall correlate the
observed loaded image to the imported code using actual load/PSP/entry provenance,
not current CS after arbitrary execution or filename alone. The normal relation
is static `0000:0100` to runtime `PSP_segment:0100`; synthesized static PSP bytes
shall be distinguished from captured guest bytes. No permanent static rebase shall
be required for each launch.

**GI-MAP-02 — Reusable static knowledge (R1 onward).** Labels, types and comments
shall remain in the static Program. Runtime changes shall remain trace bytes with
appropriate dynamic re-disassembly/freshness; copying captured/generated code into
static analysis shall require an explicit user action and capture provenance.
Mappings shall carry actual load identity and lifespan, not global permanent
address-to-file assumptions.

**GI-MAP-03 — MZ (R2a).** Reuse the MZ loader and record actual load segment, PSP,
entry, image extent, load identity and relocation relation. Map code/data sections
using the imported Program's actual bases. The same static Program shall remain
usable across two runtime load bases. Legitimate relocation-word differences
shall not reject the image via a naive full-image comparison or overwrite static
bytes. Range mapping is not relocation fixup or automatic runtime-DS decompilation.

**GI-ADV-01 — Later DOS lifetimes.** Qualified child EXEC/return, TSR, overlay and
address-reuse support shall distinguish load identities and factual lifetimes,
expire replaced mappings and retain genuinely evidenced resident code. Child
exit shall not automatically destroy all guest state. Unsupported transitions
shall be visible; no artificial process model shall substitute for native facts.

**GI-ADV-02 — Later protected/mixed profiles.** Before advertising a profile,
demonstrate its actual Ghidra decoding/navigation and native-backed translation.
Preserve visible selectors and loaded caches distinctly from current descriptor
tables; distinguish loaded-register effective translation, arbitrary-selector
lookup, page-table walk and raw backing bytes. Relevant mode/CPL/VM/default-size,
table, paging and A20 facts shall be explicit, including unavailable values.
Equal selectors may have unequal loaded caches; current tables may differ from
cached CPU translation. Effective queries shall use existing native CPU/MMU
authority without guest effects or report unsupported. Publishing descriptors alone
does not implement translation, pointers or decoding. No Lua/Java MMU is allowed;
no new language, Ghidra fork or all-mode representation is presumed approved.
The fixed trace-language choice and any HMA GUI range-boundary issue require actual
consumer qualification before extending R1's address claims.

## 9. Fresh restore

**GI-REST-01 — New lifetime (R2b).** Restore shall use the compatible fresh,
initialized disposable-target contract and create a new process and trace. Saved
PC/registers/selected bytes shall be inspectable stopped before any explicit
execution, without implicit instructions/timer progress or guest-program replay.
Record available archive/target provenance, not invented prior debugger events.
Old session/context/breakpoint identities and historical objects shall have no
authority over the new target. User-selected breakpoint intent may be reapplied
only through fresh supported operations.

**GI-REST-02 — Artifact separation (R2b).** External-host Lua, embedded Lua and Ghidra interpreter/
execution stacks, host pointers and debugger connections shall never be restored
from the machine archive. A rejected archive/profile shall produce truthful
failure and disposal status, not a usable-looking target. External saves suffice
for R2b; an in-Ghidra Save action is a separate capability subject to native
ownership/results, not implied by restore or Trace RMI support.

## 10. Failure, packaging and trust

**GI-FAIL-01 — No replay (Stage 0 onward, for each delivered effect).** Step, Continue, instrumentation creation, writes,
Save and Restore shall never be automatically resent after uncertain completion.
A complete known effect remains known even when trace publication fails; failed
publication does not roll it back. Fresh stopped reads may repair display over a
healthy channel, never replay the effect. No exactly-once or cross-system atomicity
guarantee is made.

**GI-FAIL-02 — Distinguish healthy refusal from channel loss (Stage 0 onward).** A complete typed
inspection failure requests no guest mutation and permits later fresh inspection.
A complete Pending/Active wait response permits further exact-identity observation,
not another execution. Known pre-admission refusal shall report its reason without
claiming effects. Errors after admission/reporting shall preserve known effects
and uncertainty. Incomplete/malformed framing, generation mismatch or unknown
exchange completion (including transport timeout) shall make the managed channel
unusable even for a read: no next generation, observe, resend, protocol close or
reconnect shall follow. External process disposal shall replace channel use and
report confirmation or uncertainty. An operation ID cannot repair ambiguous framing.
For Linux R1, the independent launch-control path asks the Lua host to dispose/reap
without using poisoned FD3; a dead/hung host instead uses Java's retained host
Process and the pre-armed child parent-death SIGKILL policy of GI-LIFE-04. Confirmed
host termination alone never confirms child disposal. No pidfd/descriptor transfer,
PID registry or surviving in-host emergency code is assumed.

**GI-PKG-01 — Supported composition (R1 onward; Stage 0 backend artifacts under GI-PKG-02).** Initial packaging shall target Ghidra 12.2/
Trace RMI 12.2, its required 64-bit JDK 25 and the qualified Lua 5.4 host/codec/OS composition,
starting on the qualified Linux/SDL2 managed-Lua build with matching Lua ABI/modules.
Ship the external agent, typed schema, launcher/help, required historical guard
and managed-session bootstrap/client/disposal support. The Lua client shall implement
the exact released language-neutral Trace RMI schema/framing/ordering using a qualified
protobuf library; Lua owns schema/actions/projection. The bundled Python `ghidratrace`
is reference material only, not a shipped/imported/generated client or dependency.
No Python runtime component shall be required. Agent/RPC, MCP and AI providers shall
not be runtime prerequisites. Other platforms/versions require their own evidence;
Ghidra portability or alternate RPC transport is not such evidence. Install the
integration in an isolated, version-matched environment; do not opportunistically
mutate the user's Ghidra installation. Any required version-matched guard or narrow
consumer modification shall be an explicit documented package/install choice,
not an incidental installer patch or dependency upgrade.

**GI-PKG-02 — Artifact identity (Stage 0 onward).** Stage 0 shall deliver one explicitly
versioned DOSBox-owned managed-host/client record contract and dispatch, updated
affected clients, focused lifecycle/protocol checks, managed-session documentation
and applicable package metadata/notices. Fixed non-evaluating terminal records
shall not be presented as supported by the old dependency decoder. Reuse the
unchanged dependency pin `0b8d271911c17ce762246afa86b74ab20e999343` as libraries with
its original provenance/notices; do not fork/copy its protocol, patch its installed
bytes, relabel it, add a second serving loop or probe/fallback between contracts.
Ordered generations, bounds and fatal ambiguous-exchange semantics shall apply
to both evaluator and fixed records. R1 shall ship this qualified composition.

**GI-TRUST-01 — Typed local boundary (Stage 0 onward; Ghidra surface R1 onward).** The trusted external Lua host shall
exclusively own the inherited local managed client channel, with one exchange in
flight. Ghidra actions shall use fixed typed managed records with separately encoded,
schema/range-checked data; the embedded worker's single dispatcher invokes fixed Lua
bindings, without source evaluation. Any retained evaluator-backed trusted scripting
call uses constant owned chunks and separately encoded arguments; it is not a Ghidra
invocation path. User data shall never be interpolated as Lua source, evaluated or
used to select arbitrary functions. Ghidra exposes only closed typed debugger methods,
no evaluator/console. This is not a sandbox against the trusted local host. Departure
refuses all live binding/evaluator requests before dispatch/evaluation, including races.
Native opaque contexts remain local; only native-validated copied identities cross.

**GI-TRUST-02 — Deployment and data (all deliveries, for their shipped surfaces).** Default connections shall be local/loopback;
TCP reachability is not authentication and public unauthenticated control is not
authorized. Remote use requires an explicit protected deployment decision.
Executable/configuration/image paths, including spaces/quotes, shall be passed
safely without shell interpolation. Avoid accidental verbose publication of paths,
credentials and captures; trace saving/sharing is intentional and may include
sensitive guest memory. Retain third-party notices and record actual distribution
identities; process separation alone is not a licensing conclusion.

**GI-PKG-03 — Practical interaction (Stage 0 control scheduling; R1 onward UI/trace publication).** Stop publication shall prioritize essential
registers, PC bytes and selected stack/data; memory/table capture shall be bounded
and on demand, not full dumps per instruction. Publication may be batched, but
guest effects shall preserve order and identity. Refresh bursts shall not starve
Interrupt. Trace database transactions shall be short observation updates, never
held across a long native Run or spanning a guest execution/effect request. They
shall not make guest effects atomic, undo them on abort, or imply rollback when
publication fails. No latency/performance number is claimed before measurement.

## 11. Compact traceability and acceptance matrix

Analysis references below are to the current companion identified in §1. Each scenario
shall retain the exact combined build/package/profile identity and factual outcome,
including failures. Use actual consumer paths plus focused ownership/conversion
checks; do not substitute mocks or demand exhaustive device/transport coverage.

All deliveries also undergo a focused scope/claim check against GI-SCOPE-01,
GI-SCOPE-02 and GI-LIMIT-01: the advertised workflow, documentation and action
inventory must match the delivered stage, preserve independent R2a/R2b delivery,
and claim none of the excluded capabilities. A2 demonstrates the COM product
objective; A6/A7 extend it independently. This shared review gate does not require
one extra runtime scenario per requirement.

| Acceptance ID / delivery | Actual scenario and externally observable pass criteria | Requirements / analysis rationale |
|---|---|---|
| A0 / Stage 0 | Actual managed session, Agent disabled, condition-free real-mode loop: Start returns admission before long-run completion; same channel observes and Pauses Active. Observe may see Pending/Active/Terminal; a native barrier creates a Pending opportunity, not guaranteed transport visibility. Test at native consumption that Pending Pause refuses without forced activation; gate UI truth, not a deterministic Pending sample. Read the settled revision, observe terminal twice, release, reject old identity against successor. Repeat equivalent scripted behavior via sync helper; breakpoint/manual stop remains continuable. Prove one existing callback and pending-event close on the shared path before claiming callback migration; reject callbacks in external R1 profile. | GI-LIFE-01–02, GI-OP-01–07, GI-OBS-01, GI-PKG-02; analysis §§8–10, 13 |
| A0b / Stage 0 artifact gate | Record the delivered DOSBox-owned versioned host/client schema/dispatch identity. Demonstrate a fixed terminal record dispatched by that host, replacing this host's old dependency decoder/serve usage rather than forwarding to it or adding a second serving loop. Verify all affected DOSBox clients use the one contract; deliver updated `docs/managed-lua-session.md`, focused protocol/lifecycle tests, package metadata/notices and unchanged dependency pin/provenance. Inspect the affected `dbg.live` execution-binding migration to the singular core and preservation of unrelated input/capture surfaces. A0/A1 runtime success alone cannot pass this artifact gate. | GI-PKG-02, GI-LIFE-02, GI-TRUST-01; analysis §§8–9, 12–13 |
| A1 / Stage 0 | COM exit between exchanges and fresh-restored-root exit just after a complete Active wait: receive identical typed terminal twice, reject post-departure guest effects/reads, exact release response precedes normal child exit. Native teardown does not await a departed evaluator. Fresh restored startup permits inspection without progress before explicit execution. Disconnect in delivery window reports lost receipt and confirmed/unconfirmed disposal. | GI-TERM-01–04, GI-LIFE-04, GI-FAIL-01–02; analysis §9 departure traces/proof |
| A2 / R1 | Clean isolated/version-matched installation without opportunistic Ghidra mutation; actual Ghidra window: launch COM before mapping, then import/select/map static Program. Verify accepted/Pending is not RUNNING before native activation, Pause request is not STOPPED before settlement, and initial readiness is admitted stop rather than connection. Verify exactly one guest CPU execution thread, no host worker/audio/renderer threads and no hidden competing controller (cross-check A0/A5). Use nonaligned CS (including `1234:0100`, `AX=0200`, `JMP AX`, native destination `0x12540`), near-relative boundary and far-transfer cases; verify registers, bytes, frame-PC and SS:SP, aliases and explicit reference/p-code limitations or a qualified remedy. Set two user breakpoints from static analysis, identify actual hit, distinguish internal Step Over, Step Into/Over known call/write, Pause a continuous loop, exit/dispose. Verify enumerated lifecycle controls and absence of Detach; connected trace Close initiates disposal with confirmation separate from native exit. | GI-FLOW-01, GI-LIFE-01–04, GI-UI-01–03, GI-BP-01–02, GI-ADDR-01–03, GI-MAP-01–02, GI-PKG-01/03; analysis §§3–7, 10, 12–13 |
| A3 / R1 | Capture a page at stop 1, modify through execution, reach stop 2 without recapture: old bytes are not current-known. Historical refresh enriches only matching live stop. Repeated stopped reads leave representative registers/progress/time and applicable device sentinel unchanged; refused device read never calls its handler (focused native check acceptable). Exercise 4 KiB reads and explicit holes, running-read refusal and unsupported mode/high-memory handling. Include one logical segment-relative request crossing offset `ffff`, contrasted with a linear read: demonstrate documented wrapping or explicit refusal, never silent substitution; use qualified below-1-MiB inputs, not HMA support. | GI-FLOW-02, GI-OBS-01, GI-MEM-01–03, GI-ADDR-01–02; analysis §§4–6, 10, 13 |
| A4 / R1 | From old stopped snapshot attempt Resume/Step while live is stopped elsewhere and while running; from old running snapshot attempt Interrupt while live is stopped. Exercise toolbar/keybinding, a historical breakpoint action and historical target Terminate; no effect request reaches backend. Queue click then change selection before dispatch: refuse. Return live: valid actions work. Independently invoked launcher Dispose remains available and distinct from guarded target Terminate. Browsing history itself never invokes disposal or restore. | GI-FLOW-02, GI-TIME-01, GI-UI-02, GI-LIFE-04; analysis §5 historical invocation/publication contract |
| A5 / R1 | Real second native attachment refuses; observation/control remains under the selected managed-Lua/native owner, with no hidden controller. During condition-free Run queue a GUI refresh burst, then Interrupt: demonstrate service through sequential short status/observe/bounded-wait exchanges without draining a stopped-refresh backlog or waiting for Run to finish. Consume the one in-flight reply first; no pipelining or hard real-time guarantee over native/foreign-code/transport stalls. Record exchange order and eventual exact-operation settlement. Quotes/metacharacters remain argument data; no console/general evaluator reaches backend. Truncate an effect response: no next-generation request, external disposal confirmed or explicitly unconfirmed, no replay. Contrast complete Active wait and complete inspection refusal, which allow continued healthy-channel observation. Publication failure preserves known native effect and marks incomplete display; no trace transaction spans Run or an effect request. | GI-LIFE-01/04, GI-OP-04, GI-PKG-03, GI-FAIL-01–02, GI-TRUST-01–02; analysis §§4, 9–10, 12–13 |
| A6 / R2a | One MZ with distinct code/data/stack, relocations and near/far calls at two load bases maps to the same static Program/annotations. Relocated words and one runtime code modification remain truthful dynamic bytes with freshness/re-disassembly, without overwriting static analysis. | GI-MAP-02–03; analysis §§7, 13 |
| A7 / R2b | In actual Ghidra restore workflow, select compatible archive, receive new process/trace, inspect saved PC/registers/selected bytes before progress, explicitly continue to known result. Old handles cannot address new target. Reject one incompatible archive/profile with honest disposal. No guest replay or interpreter restore. | GI-FLOW-03, GI-REST-01–02; analysis §§11, 13 |
| A8 / later profile | Before advertising the specific increment, demonstrate actual DOS load/mapping lifespans or actual Ghidra decoding plus native translation. Select discriminating cache/table, selector, default-size, far-pointer, paging or A20 cases relevant to that profile; table objects alone do not pass. | GI-ADV-01–02; analysis §§6–7, 13 |

The following focused checks are mandatory parts of the named rows, not additional
product features or an exhaustive test matrix:

- **Lua-only revision-2 coverage (A0/A0b/A1/A2/A5):** qualify the external Lua host,
  exact 12.2 protobuf framing/negotiation, XML schema, complete typed method
  declarations, integer/register encoding and FIFO reverse invocation replies.
  Interrupt priority must not reorder those replies. Verify actual launch environment,
  installed Lua/module provenance, child/FD ownership and EOF distinct from reaped
  exit; run the integration package without Python. Existing Python test fixtures
  remain legitimate independent tests. Prove Pending activation at the held native
  frontier with Agent disabled, rather than relying on an unreachable outer loop.
  Exercise partial-frame non-destructive wake, continued terminal-only dispatch,
  native settlement before owner unwind, and failed Release flush after consumption
  without retry. Callback expiry uses a documented native-owned deadline independent
  of the seven transport budgets. These refine the existing acceptance IDs, not add
  another delivery stage or waive prior scenarios.

  A0's Start reply is admission only. Transported Observe may see Pending, Active
  or Terminal; a native barrier creates an opportunity, not a deterministic Pending
  sample. At native consumption explicitly test Pending Pause refusal with no forced
  activation; UI assertions require truthful state, not visibility of every state.
  A1/A5 cover host death before/after child arm, parent change to a non-init subreaper,
  Java disposal of a hung host, ignored TERM followed by live-host KILL, and missing
  child-exit evidence staying unconfirmed. No child finalizer or descendant-tree
  cleanup is inferred. A2 covers startup exceeding stock ten seconds but within
  120000 ms, expiry/cancellation before native-ready and after ready during trace
  creation/publication, visible stage facts and disposal. A2/A5 also cover explicit
  host-selected trace oid, open-use/close identity and non-reuse, unknown raw reason
  → unclassified with explicit continuation only, and invalid terminal schemas/enums
  → protocol failure. These remain implementation acceptance obligations, not results.

- **A0 — identity, callback and caller migration:** receive session identity at
  creation/readiness before Start; check its binding on subsequent responses.
  Observe/wait of an ordinarily released identity explicitly refuses. In the
  callback proof, guest execution remains held while the provisional event is
  pending; callback reads equal the hit-time state, not post-resume state. Exact
  verdict or error/expiry/close settles authority before execution can proceed.
  Exercise the affected `dbg.live` execution bindings through the consolidated
  core as well as the managed helper, with no old execution engine providing the
  passing path. Breakpoint/manual stop permits a subsequent valid operation.
  Include a focused regression of unrelated supported live input/capture behavior,
  reusing unchanged evidence where applicable. **GI-LIFE-02–03, GI-OP-02/05/07**;
  analysis §§8–9.
- **A1 — ordered departure edges:** a bounded native/session ownership check shall
  arrange known departure with no selected operation (for example after exact held
  release, using the existing native departure seam, not a new R1 target action).
  Session-addressed observation returns only its departure fact, no fabricated
  operation/release receipt; Close ends delivery. Exercise both release/departure
  consumption orderings: one ordinary-release response plus session-only departure
  when release wins, or one copied-slot-release response and closure when transfer
  wins. No duplicate response, lost already-decided response due to transition,
  double native release or blocked native teardown is permitted; transport loss
  remains separately uncertain. An in-flight wait may truthfully return Active
  before later terminal observation. **GI-LIFE-03, GI-OP-02/05, GI-TERM-01–04,
  GI-FAIL-02**; analysis §9 departure ordering/traces.
- **A2 — actual consumers and reuse:** verify initial guest thread/frame activation
  and visible Registers/Listing/frame PC, the mapped COM Module and Memory Map,
  and both user registrations in the Breakpoints window. Do not accept schema
  population alone or demand MZ sections. Compare captured observations from two
  separate COM launches while reusing the same static Program/annotations, with
  distinct live identities and no historical execution. **GI-UI-01, GI-FLOW-02,
  GI-MAP-01–02**; analysis §§3–5, 7.
- **A3 — capture bound:** verify stop publication captures essential registers,
  PC bytes and selected stack/data, leaving unrequested ranges unknown; a stop
  shall not trigger an unconditional full-RAM/descriptor dump. Additional capture
  follows explicit bounded demand. **GI-PKG-03, GI-MEM-03**; analysis §§4, 12.
- **A5 — local deployment/data:** verify shipped default endpoints are local/
  loopback, no public unauthenticated control listener is introduced, and use/help
  describes intentional trace saving/sharing and potentially sensitive guest
  memory. Check ordinary logs/projections for accidental verbose disclosure of
  configuration paths or credentials without publishing sensitive test data.
  **GI-TRUST-02**; analysis §12.

## 12. Exclusions and review gates

**GI-LIMIT-01 — Non-goals.** This contract does not authorize concurrent debugger
clients/control transfer, a new emulator or MMU, generic remote Lua evaluation,
GDB/RPC fallback integration, reverse execution, in-place machine load, interpreter
serialization, universal device inspection, arbitrary extenders, full unwinding,
BIOS/external-boot debugging, automatic attachment by filename, or a universal
segmented decompiler. It does not authorize implementation, deployment, runtime
action or changes to consumers such as PM.

Technical design and subsequent qualification must still establish the exact
historical-action guard hook, coherent async/event/departure lifetimes, native
mode/load/hit projection, client/package contract and Ghidra freshness behavior.
The nonaligned-CS consumer experiment must settle truthful limitation versus
necessary narrow remedy; advanced representation remains a later evidence-led
decision. These are unresolved means/proofs, not permission to weaken the above
behavior or to claim current support. The earlier revision-2 review remains historical
evidence for its original text. Independent Lua-only revision-2 approval covers the
Lua Trace RMI codec/ordering, typed FD3 client and exact process-owner placement,
preserving the reviewed native/terminal/historical races; their implementation proofs
remain outstanding.
Neither implementation nor runtime/product qualification is claimed.
