# Ghidra integration: functional and technical analysis

**Date:** 2026-11-10
**Status:** independently approved functional/technical analysis; owner-selected async Lua architecture with terminal-only delivery. Not implemented or runtime-qualified.

**Purpose:** define what Ghidra needs from DOSBox-Y, what Trace RMI already supplies, and the smallest coherent path to an excellent DOS reverse-engineering experience.

## 1. Conclusions

1. Integrate DOSBox-Y as a **Ghidra Debugger backend**, not as a host process to debug with GDB and not as a new emulator inside Ghidra.
2. Select an external Python **Trace RMI** adapter over an **asynchronous managed Lua debugger interface**, using the existing exclusive native owner. This is the owner's architecture decision. Evolve the current blocking Lua bindings into one async core with synchronous convenience wrappers implemented on top; do not run parallel old/new execution engines. The async interface is required work, not an existing capability (§9).
3. Transport is not the main uncertainty. Correct **logical addresses, segment caches, program-counter tracking, code mode and static/dynamic mappings** determine whether the result is genuinely useful.
4. Trace RMI can carry descriptor/cache observations as typed objects and registers. It has **no built-in GDT/LDT synchronization or x86 translation operation**. Publishing tables alone changes neither address translation nor pointer interpretation, disassembly or decompilation.
5. Ghidra 12.2 already supplies real-mode and 16-bit protected-mode languages, COM/MZ loaders, time-indexed traces, breakpoints and static mappings. These are valuable foundations, **not automatic support for live segmented DOS execution**.
6. Ship a **packaged, useful real-mode COM debugger first**, including launch, static mapping, breakpoints, execution and safe observation. MZ mapping and fresh restore are independent later increments. Protected/mixed mode is a separate delivery, not a condition for shipping the first three. A narrow version-matched Ghidra action guard is required for historical-view effect exclusion (§5); other extensions need a demonstrated consumer gap.
7. Side-effect-free reading and CPU-effective address translation are separate properties. The existing native inspector provides important safe primitives, but its backing-memory/page-table reads do not establish A20, loaded-cache or stale-TLB equivalence (§6). Do not turn a useful inspector into a claim of universal CPU mapping.
8. This document authorizes no implementation, deployment or live runtime action. Preserve ordinary supported DOSBox behavior while updating affected Lua callers to the singular core; wrappers preserve synchronous convenience, not a second execution path or compatibility-shim layer. Unrelated frontends are not removed.

## 2. Evidence baseline and limits

### Pinned inputs

| Component | Researched input |
|---|---|
| Ghidra | Latest published release found on 2026-11-10: **Ghidra 12.2**, tag `Ghidra_12.2_build`, published 2026-10-22; source commit `8ca9077040cc43cd790babcfccf19475622a3e9a` |
| Trace RMI | Release source above; Java handler and Python client both declare protocol version **12.2** |
| DOSBox-Y | `bd053ee4e8a5b258191b3f90212244f7bd4066e7`, branch `original-savestate-restart`; production managed-host input `4ec98bc6bf65aa64910df126cb9089958a6c7527` |
| Existing architectural boundary | One exclusive native debugger attachment, fresh initialized disposable restoration, no Lua interpreter serialization |

The research inspected actual protocol, Python agents, Java GUI/model consumers, address implementations, language definitions, loaders, and DOSBox-Y native/RPC/Lua sources. References are pinned in §15. **Source facts** below describe those inputs; **requirements** describe proposed delivered behavior; **prototype obligations** are explicitly unresolved. This was source research, not a running Ghidra GUI prototype. Existing qualification documents are reports of earlier work, not checks rerun here. The release publication date is recorded research evidence, not a guarantee about subsequent releases.

The older `docs/rpc-development.md` is explicitly a draft and has historical Windows-only and architecture text. Current source and current tested contracts take precedence over assuming every statement in that draft describes the current implementation.

## 3. Functional objective: what “well integrated” means

The operator should be able to import a DOS executable, launch or attach to its stopped DOSBox-Y instance, and use Ghidra's existing analysis and debugging views together. Debugging the C++ DOSBox process is a different activity and is not this feature.

### Principal workflows

**A. Observe a stopped program.** Import a COM/MZ file, select the corresponding runtime module and see its entry instruction, segment registers, flags, stack bytes and data. Moving around the listing or expanding Model objects must not execute instructions, acknowledge hardware, or modify the guest.

**B. Debug using the static analysis.** Add a breakpoint at an analyzed function, continue, stop there, step into/over and inspect a changed variable. Jump between dynamic code and the imported program's labels, comments, cross-references and decompiler. Maintain the distinction between runtime bytes and the original image.

**C. Inspect a restored experiment.** Restore a save into a fresh disposable instance, attach while it is stopped, inspect it and continue only on an explicit command. The agent and Ghidra are newly initialized; the old Lua stack, debugger connection and host pointers are not restored.

**D. Investigate DOS-specific behavior.** Follow far calls, changed DS/ES values, interrupts, child EXEC, resident code, overlays and self-modification without assigning ordinary host-process/thread semantics to them. Advanced cases can be delivered incrementally, with unsupported transitions made visible.

**E. Repeat and compare.** Run the same small workload in separate instances and compare captured registers/memory. Keep analysis annotations reusable. Trace snapshots are observations; selecting an earlier snapshot never secretly restores or continues the machine.

### User-facing requirements

| ID | Requirement | Required behavior / boundary |
|---|---|---|
| F01 | First-class launcher | Ghidra launcher selects DOSBox executable, explicit configuration, guest image/program or restore archive, and connection mode; paths with spaces work without shell interpolation bugs. |
| F02 | Stopped entry | Initial publication happens after actual stopped native admission, not merely socket connection or process creation. |
| F03 | Correct CPU display | General/segment registers, flags and true logical IP/EIP; no flattened address disguised as a CPU register. Add FPU/control registers when actually exported. |
| F04 | Correct current instruction | Dynamic Listing and top frame follow the effective CS:IP/EIP location, including far jumps and segment aliases. |
| F05 | Safe memory inspection | Explicit logical/linear/physical meaning; unreadable/MMIO ranges remain unavailable, not zero-filled or read with guest side effects. |
| F06 | Execution control | Continue, interrupt and native step-into/over with actual stop reasons; retain unrestricted Run rather than an invented instruction ceiling. |
| F07 | Breakpoints | Real supported execute/watchpoint kinds only; actual native identities, temporary status, deletion and hit attribution. “Memory changed” is not a read/access watchpoint. |
| F08 | Static/dynamic correlation | Module/section mapping with actual load provenance and lifespan; no permanent rebasing of an analysis project to follow each launch. |
| F09 | Segmented navigation | Preserve selector/segment and offset alongside translated location; allow explicit segment-register-relative inspection. |
| F10 | Time and freshness | Historical views cannot send effects to the live target; refreshed bytes belong to the actual current stop, not the selected historical snapshot. |
| F11 | Lifecycle | Exit, failed restore, disconnect and detach have distinct outcomes; a second frontend is refused under existing exclusive ownership. |
| F12 | Honest analysis | Frame zero is valid without an invented full call stack; no fabricated symbols, locals, threads or call-return history. |
| F13 | Optional edits | If exposed, register/memory edits are live-target effects, stopped-only, supported by the native API and read back. Trace edits alone are not guest edits. |
| F14 | Reusable knowledge | Existing Ghidra labels/comments/types remain in the static program; copying runtime code into static analysis is an explicit user operation. |

### Scope by delivery

The table above is the overall product direction, not a single release gate:

| Delivery | Included | Explicitly later |
|---|---|---|
| R1: useful real-mode COM debugger | Managed launch at entry; ordinary real-mode RAM below 1 MiB; true registers and frame-zero PC; explicit SS:SP navigation; manual verified COM mapping; native execute breakpoints, Run/Interrupt/Step Into/Over; history and process disposal; installable launcher/help | Attach to an existing controller, MZ automation, restore, target edits, watchpoints, stack unwinding, HMA/unreal/V86/protected mode |
| R2a: MZ analysis usability | Real-mode MZ sections, load/relocation provenance, reusable static analysis at different load bases | General DOS loader tracing, arbitrary overlays and extenders |
| R2b: fresh restore usability | New stopped disposable target and new trace; inspect then explicitly continue; supported archive/profile only | In-place load, reverse execution, serialized debugger/Lua state |
| R3: advanced execution contexts | Individually qualified protected16/protected32/mixed-mode observations and navigation; additional DOS lifetimes | Universal extender/device coverage or a new CPU engine |

R2a and R2b need not wait for one another. Each ships with its own packaging and meaningful acceptance, not a final all-feature installation milestone. R1 may reject high-memory requests rather than pretend A20 wrap is solved. Its limited mode/cache observation must distinguish the ordinary-real-mode interpretation it relies on; the current `cpu_mode` string alone is not proof that hidden caches are ordinary. At an unsupported stopped context, retain raw observations and permit safe interrupt/disposal, but disable misleading decoding, mapping and address-sensitive effects. Continuous Run need not stop at every transient mode transition; no such monitoring guarantee is implied.

“Best integration” means that each advertised workflow works coherently, not just that the Model tree contains properties.

## 4. What Trace RMI actually does

### 4.1 Two directions over one connection

Trace RMI is Ghidra's debugger integration protocol, **not Java RMI**, JSON-RPC or MCP. Its protobuf messages implement two complementary flows:

* **Agent → Ghidra:** create a trace with a language/compiler pair; create/insert typed objects; publish registers, memory bytes, memory-known/error states and snapshots; activate a selected object.
* **Ghidra → agent:** invoke registered typed remote methods such as `resume`, `interrupt`, `step_into`, `read_mem` and breakpoint actions. The agent performs the native operation and publishes the resulting trace observations.

Do not confuse `put_bytes`/`put_registers` with writing the guest. They write Ghidra's trace database. Actual target modification requires an advertised remote method backed by a supported DOSBox operation.

Use the bundled `ghidratrace` package: `Client`, `MethodRegistry`, `Trace`, `Address`, `AddressRange`, `RegVal` and the object schema. Do not write another protocol implementation. Remote method names alone are insufficient: action hints, typed parameter schemas and canonical object traversal determine which GUI actions bind.

### 4.2 Initialization and connections

Recommended local sequence:

1. Ghidra's launcher creates a loopback, ephemeral acceptor and starts the Python agent with its endpoint.
2. The agent negotiates Trace RMI 12.2 and connects to/launches its selected native backend.
3. It obtains the actual exclusive stopped target through managed Lua startup, selects the matching language/compiler, creates the trace and its minimal object root. Startup readiness and execution admission are distinct from CPU progress.
4. It publishes the initial stopped snapshot and activates the actual thread/frame.

Ghidra also supports reverse connection arrangements. TCP client/server direction is distinct from the Trace RMI agent/server roles. Prefer the normal launcher path initially rather than introducing discovery or a second listening service.

The R1 launch offer must permit launch before manual static mapping: select `requiresImage() == false` and a no-task auto-map policy for this offer, while allowing explicit selection of an imported Program afterward. `AbstractTraceRmiLaunchOffer` otherwise can require a Program and wait/fail on missing automatic mapping before the manual workflow is possible. Prove launch completes with no automatic mapping, then perform the documented Map Module/Section action. Do not assume every stock launcher has this contract.

The protocol version is explicit. Package and test against the selected Ghidra release; do not assume a newer Ghidra accepts an arbitrary older `ghidratrace` wheel.

### 4.3 Trace operations and transactions

Use short trace transactions and batching for one observation update. Create objects, assign attributes, then insert their canonical lifespan. Remove/expire objects and mappings when their factual lifetime ends.

These are **Ghidra database transactions**, not transactions over guest execution. Aborting a trace transaction does not undo a step, memory write, file effect or save. Never retain a database transaction throughout a long guest Run or imply an uncertain native effect rolled back.

### 4.4 Registers and memory

* Trace RMI register values are **big-endian byte arrays regardless of target endianness**. x86 guest memory remains little-endian. Register names/widths must match the selected Ghidra language; inspect `put_registers` skipped names rather than silently losing state.
* Create a register overlay space associated with the actual RegisterContainer path, following Ghidra agent conventions. A Model property called `AX` is not by itself a populated register bank.
* Memory has `unknown`, `known` and `error` states. Publish successful bytes and errors accurately; an inaccessible byte is not a real zero.
* The RMI handler must accept a 4096-byte automatic read request. The native Lua read maximum is currently 4096; larger requested ranges are split, and a smaller future configured per-call limit requires complete bounded subreads inside the adapter rather than rejecting every GUI block. Publish exact successful subranges and explicit holes; never silently report a shortened read as a complete block. Without safe range information, a failed whole-block read may leave the block unavailable. A native minimum of 4096 is not intrinsically necessary if the adapter actually performs that splitting.
* Ghidra 12.2's live memory reader quantizes requested ranges into **4 KiB blocks** (`TraceRmiTarget.BLOCK_BITS = 12`). A visible few bytes may cause a larger backend request. Bound/split reads against native request limits and known readable boundaries. Lua's native linear inspector stages a complete range or fails; RPC independently follows whole-range publication. Publish only separately successful subrequests, never bytes from a failed response. If a requested page cannot be safely subdivided using known range information, report it unavailable; do not scan it byte by byte or fall back to device reads.
* There is also a **pre-dispatch Ghidra boundary hazard**: `FFFF:FFF0` is flat `0x10ffe0`, within real-mode maximum `0x10ffef`, but `quantize()` constructs rounded maximum `0x10ffff` in Java before invoking `read_mem`. The address constructor rejects that out-of-range value. Agent-side splitting cannot fix a request it never receives. This affects the final partial 4 KiB block, not every HMA address. R1 excludes HMA; a later HMA delivery needs a focused GUI reproduction and, if confirmed in that path, a narrow Ghidra-side range clamp or supported alternative read path as well as native A20 correctness. No GUI reproduction is claimed here.
* Capturing one page does not certify every other page at that stop. On execution, ensure old known data is invalidated appropriately at new snapshots before it can masquerade as fresh observation; retain historical data at its old time. Ghidra 12.2's `DorkedRequestCaches` forwards reads without caching: this is a trace-memory inheritance issue. The source-backed candidate is `Trace.set_memory_state(range, 'unknown', snap)` over the advertised memory ranges at the new snapshot, then publish only that stop's successfully observed subranges as known and actual failed reads as error. Do not delete historical bytes or label unrequested ranges error. Validate actual listing/time behavior and range coverage; API existence or incrementing the snapshot number does not prove freshness.

## 5. Ghidra object and GUI contract

### Minimal useful schema (proposed names)

```text
DOSBoxSession                         EventScope / FocusScope / Aggregate
  Machine                             custom observed metadata, mode and A20
  Processes[guest]                    Process / ExecutionStateful / Aggregate
    Environment                       Environment
    Threads[cpu0]                     Thread / ExecutionStateful / Aggregate
      Stack[0]                        StackFrame; PC is an ADDRESS
        Registers                     RegisterContainer
    Memory[region]                    MemoryRegion
    Modules[load]                     Module
      Sections[section]               Section
    Breakpoints[id]                   BreakpointSpec / BreakpointLocation
  Segments[CS|DS|ES|SS|FS|GS]          observed selector and loaded cache
  DescriptorTables[GDT|LDT|IDT]        paged/on-demand observed entries
```

These path names are a proposal, not existing DOSBox methods. Use canonical container schemas and required interfaces/attribute aliases from the selected Ghidra release. In 12.2, examples include `_state`, `_pc`, `_range`, `_enabled`, `_kinds`, `_module_name`, `_focus` and `_event_thread`. Ordinary descriptive field names can use schema aliases; arbitrary names without interfaces will not populate the corresponding debugger windows.

Model one **guest CPU execution thread**, not the host renderer, audio and Lua worker threads. A DOS PSP/program activation is not necessarily a separately schedulable OS process. Begin with one controlled guest execution context and represent DOS load/child/residency metadata separately. Add multiple Process objects only if their lifetime and address meanings are defensible.

Publish the actual top frame with its translated PC. This does not claim stack unwinding: near/far returns, interrupts/IRET, hand-written assembly and DOS extenders need separate unwinding evidence.

The Dynamic Listing consumes trace bytes, language/context and tracking; Registers consumes the register overlay; Stack consumes frame objects; Modules/Memory Map consume their respective interfaces; static listing synchronization consumes explicit static mappings. The decompiler normally analyzes the static Program. A Module object or a changed DS property does not dynamically relocate its decompiled pointers. Watches and p-code emulation are analysis consumers whose segmented semantics must be qualified separately, not advertised as a DOSBox hardware oracle.

### Mapping actions to backend functionality

| Ghidra action | DOSBox-Y owner / requirement |
|---|---|
| `refresh` registers/environment | Lua stopped-context/register bindings extended with required metadata; actual native admission |
| `read_mem` | Lua native-backed linear inspection, with explicit address meaning and safe-region limits; RPC modes are evidence, not callable Lua methods |
| `resume` | Proposed async Lua start(Run); show accepted/pending until native activation is observed, not presumed CPU progress |
| `interrupt` | Proposed Lua Pause for the selected native operation; report stopped only after its terminal observation |
| `step_into`, `step_over` | Proposed async Lua start(Step/Over) over existing native execution; no Python instruction loop or disassembly-based stepping |
| `break_sw_execute` or appropriate supported execute action | Lua native breakpoint registration; label according to actual mechanism/build, not an arbitrary UI convention |
| `break_write`, `break_read`, `break_access` | Only advertise exact native support; existing MemoryChange is not automatically any of these |
| enable/disable/delete breakpoint | Bind only operations actually supported; do not present nonexistent toggling as successful |
| `write_mem`, register editing | Not R1; RPC/native edit capabilities are not automatically exposed by selected Lua bindings. Add only through the same native owner if later commissioned |
| detach / terminate / close trace | Separate meanings; closing a trace does not itself settle the native owner or kill the emulator |

Do not register step-out, reverse-step, hardware breakpoints or arbitrary watchpoint conditions as inert buttons. Optional actions become available after a native-backed implementation and relevant evidence.

Advertised RMI actions must be derived from the selected Lua API's actual implemented operations and native build/core/context capabilities, with admission rechecked at use. Project the necessary facts in the existing session/status response; do not invent a capability registry. RPC `agent.capabilities` and `dbg.live.status` are useful existing examples, not interchangeable capability authorities for the FD3 worker. An available RPC method or read-only diagnostic command is not permission to expose a Lua effect. R1 registers neither `write_mem` nor register-edit target actions.

Register **no** free-form command method, `ActionName.EXECUTE` handler or Ghidra console forwarding path. `TraceRmiTarget.executeAsync` forwards console strings to a matching remote method; the GDB agent's execute method is not a pattern to copy here. Only the closed typed debugger method set is exposed; internal Lua evaluation is owned by the trusted adapter (§9).

### Historical-view effects: required Ghidra-side action guard

The inspected stock action path does not establish F10: `TraceRmiTarget.ActionEntry.isEnabled` uses `getSnap()`, which returns the connection's **last published snapshot**, not the user-selected historical snapshot. Thus the claim that an old STOPPED view necessarily enables Resume while the live state is RUNNING is not supported by this source. The actual gap is that ordinary resume/interrupt/step signatures carry no selected time, and `ActionEntry.invokeAsyncWithoutTimeout` invokes the method without a selected-versus-live check. `TargetDockingAction` selects the current target and delegates to the entry; native/current-state enablement alone is not historical-view exclusion. A backend current-operation check cannot distinguish a live-view click from a historical-view click targeting that same live process. Focus-only `activate` prevents implicit time travel but does not guard Resume.

**Selected mechanism:** ship a narrow version-matched Ghidra-side guard on effect invocation, preserving standard debugger actions only behind that guard. It compares the actual selected trace/time with the connected target's published live trace/time, checks live target identity and appropriate current native state, disables effects while historical, and repeats the check at invocation before sending any effect request. Apply it to toolbar, keybinding, context-menu and breakpoint/edit/terminate effect paths, not merely button painting. Selection changes before dispatch invalidate a queued action; a valid click already dispatched while live is not retrospectively cancelled by subsequent browsing. Focus-only `activate` is read-only and declares neither `snap` nor `time`; it never executes, restores or changes the backend's authority. Explicit launcher-owned disposal remains available independently of historical-view debugging actions.

The implementation boundary is the Ghidra consumer's action invocation/dispatch path (`ActionEntry.invokeAsyncWithoutTimeout` and corresponding built-in effect paths), not a new Python heuristic or an assumed parameter the stock methods do not transmit. Technical design must establish the exact extension hook; if the stock plugin exposes no adequate hook, package the narrowly scoped version-matched consumer modification. **Do not register usable unguarded standard effect actions or ship without the guard.** Backend exact-context/operation admission remains a separate race check, not a substitute. This guards ordinary operator selection, not a hostile local client directly invoking remote methods. No new authorization-token registry is needed.

R1 acceptance must attempt Resume/Step from an old stopped snapshot while the live target is both stopped elsewhere and running, and Interrupt from an old running snapshot while live is stopped. Exercise toolbar/keybinding and a historical breakpoint action: none may reach the backend as an effect. Returning to live selection must restore the applicable actions. A queued click followed by a selection change before dispatch must also be refused. Independently test historical refresh using the snap-keyed publication rule below; this is an acceptance case, not a named existing Ghidra guard.

**Refresh publication mechanism:** refresh/read methods take no selected historical time. The adapter binds the observed native session/stop identity to its corresponding live trace snapshot and publishes bytes, registers and memory states using explicit `snap` arguments to `Trace.put_bytes`, `Trace.put_registers` and `Trace.set_memory_state`; object updates use that snapshot's lifespan. These APIs otherwise default to the agent's `self.snap()`, not the GUI selection. Explicit snapshot selection and the serialized same-stop observation rule (§10.4) prevent an asynchronous refresh from accidentally using a later agent snapshot. Never derive publication time from the historical UI cursor or retarget a stale batch to the next stop. R1 must demonstrate that refreshing with history selected enriches only the matching live-stop snapshot and leaves the historical snapshot unchanged. This publication discipline is independent of the effect invocation guard and remains implementation/consumer-proof work.

For R1, publish native execute locations and deletion honestly. The Lua driver uses `Debugger_ResolveAddress` and `Debugger_BreakpointSet`; the Agent path and `CBreakpoint::CheckBreakpoint` independently show resolved-location semantics, not simply a persistent CS-selector predicate. A static-listing click must use the selected module mapping and a qualified native conversion, not Ghidra's normalized segmented spelling as original CPU coordinates. Ordinary real-mode aliases should resolve to the same advertised execution location in the R1 fixture. Beyond that profile, mapping/table/A20 changes require explicit semantics before such breakpoints are offered. Advertise one-shot/toggle operations only if the selected Lua binding actually provides them; native capabilities in another frontend are not sufficient. Any native removal must expire the corresponding trace location. Unsupported enable/disable is absent, rather than emulated by hidden delete/recreate effects.

**Required hit projection:** retain the exact session-scoped Lua registration identity, native breakpoint/watchpoint ID, owner/kind, temporary/internal status, hit count where tracked, and raw stop reason at the actual native hit producer; carry the applicable facts through settlement and terminal-copy delivery to the trace. The current non-observer hit path reduces an ID to `local_driver_hit`, and terminal projection emits only `reason="breakpoint"`. `Debugger_RecordBreakpointStop` records hit counts for Lua but its full stop event is step-gated; neither is already the needed continuous-Run projection. Internal step-over/run-until temporary breakpoints must be reported as their actual internal completion/condition, not attributed to a user's breakpoint by matching PC. Unavailable attribution is explicit, never guessed. R1 must demonstrate two armed user execute breakpoints, link the actual hit to the clicked location/registration, and distinguish an internal step-over stop. Later watchpoints require the same identity contract without expanding R1's action set.

## 6. Segmentation: responsibilities and exact hazards

### 6.1 What is carried on the wire

`Addr` is `{space: string, offset: uint64}`. `AddrRange` uses a named space, offset and extent. There is no extra segment field. Generic typed objects can carry selectors, bases, limits, attributes, raw descriptor bytes, table registers and validity/error status, all at a snapshot/lifespan.

**Important limit:** Trace RMI can create overlays over existing spaces; it does not offer a general “create an arbitrary-width address space” operation. The chosen trace language defines base address spaces. A custom object named `PhysicalMemory` does not create such a space.

### 6.2 Real mode: representation versus machine translation

Ghidra's `x86:LE:16:Real Mode` has a segmented `ram` address space whose underlying flat offset is:

```text
flat = (segment << 4) + offset
```

Its range ends at `0x10ffef`; it does not represent arbitrary 32-bit extended physical memory. Its default compiler ID is **`default`** (the 32/64-bit agent examples' compiler choices must not be copied blindly).

`1234:0010` and `1235:0000` both encode flat `0x12350`. Trace RMI transports this flat address, and Ghidra reconstructs a normalized segmented spelling. **The original alias is not preserved by the wire address alone.** Preserve CS/DS/etc. and requested logical coordinates as explicit metadata. Use ordinary segment-relative navigation for operations where the exact register/cache association matters.

Do not apply `& 0xfffff` globally. A20-disabled bus addressing, A20-enabled high memory and real/unreal-mode hidden-cache behavior belong to DOSBox. Logical, linear and bus-physical addresses are distinct. R1 limits trace memory to qualified ordinary real-mode ranges below 1 MiB; it does not infer arbitrary effective addresses from visible segment values. Explicit `segment:offset` requests also need defined offset wrapping: current Agent segmented reads mask each real-mode byte offset to 16 bits, whereas a flat linear trace read advances linearly. These are different requests, especially across `offset=ffff`.

### 6.3 A concrete Ghidra UI trap: PC and SP

The 16-bit processor specification declares **EIP** as the program-counter register. Ghidra's generic register location tracker turns that value directly into an address; it does not combine CS with EIP there. Publishing a correct register bank alone can therefore navigate to offset `0100` rather than the actual `CS:0100` code.

Ghidra's **Auto PC** first consults the stack frame PC for snap-only coordinates, then falls back to the register tracker. Publish a truthful frame-zero `PC` ADDRESS, preserve real EIP, and verify that normal Auto PC works across far transfers. Never “fix” navigation by replacing the EIP register value with a flattened CS address. The raw register-only tracking mode remains a known distinction.

SS:SP stack navigation has the analogous issue. Start with explicit effective stack navigation; if needed, add a focused Ghidra `LocationTrackingSpec` implementation for segment-aware PC/SP. The prototype must decide whether frame-PC plus standard UI suffices for normal operation or a small extension is justified.

Correct PC highlighting is necessary but insufficient. In `ia.sinc`, real-mode `currentCS` explicitly computes `(inst_next >> 4) & 0xf000` and assigns that value to its CS operand. Indirect near CALL/JMP use `segment(currentCS, rm16)`; `rel16` wraps the destination within a normalized flat 64 KiB window. Publishing the real CS register or adding a `LocationTrackingSpec` does not repair those SLEIGH semantics.

A discriminating R1 experiment is code at **1234:0100**, `AX=0200`, executing `JMP AX`: native destination is flat `0x12540`; the synthesized `currentCS=1000` would instead yield `0x10200`. Also inspect a near relative branch crossing the normalized flat 64 KiB boundary without crossing the actual CS-relative boundary, plus a far transfer. Check native destination against actual Ghidra references and any advertised evaluated target. This is a source-derived hazard, not a measured GUI result.

R1 can still ship correct bytes/instruction display, PC navigation and **native** stepping with affected dynamic control-flow/reference or p-code features explicitly unqualified and not offered as reliable navigation. A correctly mapped static Program can provide analysis where its own segment assumptions are valid; mapping alone is not proof. If the normal UI cannot make that limitation clear enough to avoid misleading users, a narrow projection/context/language-side remedy is required for that workflow. Choose the smallest remedy demonstrated by the experiment; a tracker-only change is not such a remedy. Universal segmented decompilation or a new all-mode language is not an R1 gate, and native disassembly text is not proof of Ghidra Listing semantics.

### 6.4 Protected mode is not solved by choosing a language

Ghidra's **16-bit Protected Mode** address class encodes `(selector << 16) | offset16`. Its processor segment operation uses that encoding. This preserves selector identity; it **does not consult the live GDT/LDT or loaded segment caches** and is not a physical/linear translation algorithm. It also does not encode arbitrary selector plus 32-bit EIP in that 32-bit address.

For eventual protected-mode correctness, require:

* CPU mode, CPL, VM state, operand/address sizes and CS D/B; distinguish real, V86, protected16, protected32 and relevant unreal-mode cases.
* Loaded segment-register **visible selector plus cached base/limit/access/default-size/validity**. A descriptor table entry changed since loading must not replace the effective cache.
* GDTR, IDTR, LDTR and task-register observations with actual bases/limits/selector/cache as supported; paged, bounded GDT/LDT/IDT descriptor inspection, not unconditional full dumps on every stop.
* Paging controls (including CR0/CR3/CR4 and supported large-page mode) and effective A20 configuration; translation queries serviced by DOSBox's native inspection with explicitly qualified meaning.
* Distinguish **loaded-register addressing** (e.g. DS plus offset using its cache) from **arbitrary-selector lookup** against current descriptor tables. The same visible selector need not imply the same effective base in every loaded register.
* Explicit failure for invalid/unavailable translations and MMIO. Introspection must not load segment registers, raise guest faults, set accessed/dirty bits or run the guest to discover a mapping.

Publishing the tables satisfies transport/Model inspection only. Segment-aware Go To, breakpoint conversion, pointer interpretation and disassembly context must **consume** the metadata. Existing Ghidra static mappings express range translations over lifespans; they are not a substitute for the CPU's general descriptor/page translation.

### Native inspector limits that change the design

At the pinned baseline, `PAGING_InspectLinear` reads present PDE/PTE bytes (or a supported large-page entry), or uses `paging.firstmb` with paging disabled. It does not consult an already cached CPU TLB translation or simulate access-right checks. Its own request limit is 4096 bytes (`paging.cpp:970–972`), as is managed Lua Read admission (`debug.cpp:6238–6239`). Separately, `MEM_InspectPhysicalMemory` delegates to `InspectPhysicalMemory`, whose explicit `length>4096` rejection enforces the same per-call cap (`memory.cpp:875–891` at `bd053ee4`). That helper copies `MemBase + address` only for existing RAM/ROM page handlers, with a 16 MiB ceiling. It does not follow the ordinary page handler's A20 alias masking. `MEM_A20_Enable` changes that alias mask and clears the ordinary TLB; this does not add equivalent masking to the raw inspector.

These facts support **side-effect-free backing-memory/table inspection**, not the stronger assertion “these are always the bytes the CPU would fetch.” A changed page table without the applicable invalidation is another possible divergence, just as a rewritten GDT entry can differ from a loaded segment cache. No runtime reproduction of either divergence was performed here. They are concrete source-based limitations to resolve or exclude before broader claims.

The advanced native contract must identify whether a query means (a) loaded-register effective translation, (b) arbitrary-selector/current-table translation, (c) current-page-table walk, or (d) raw physical backing bytes. For effective execution mapping, use the emulator's relevant existing cache/translation state without filling caches, changing accessed/dirty bits or faulting the guest; otherwise report that query unsupported. Report guest access permission separately from debugger inspectability. Do not duplicate an x86 MMU in Python/Java, and do not silently strengthen the meaning of existing public `memory.read` modes. A focused additional query is legitimate when existing semantics differ.

### 6.5 Recommended address strategy and architecture gate

**First delivery:** a real-mode trace using the existing real-mode language/compiler, with native-backed flat linear reads within the R1 supported range and explicit logical address metadata. Do not feed the RMI offset through an arbitrary inverse segment conversion when an exact logical request is needed. Keep a trace address's meaning stable: `ram` is the chosen linear observation view, not sometimes selector encoding and sometimes physical memory. Expose other coordinate queries explicitly; do not fabricate a new base address space merely by publishing a Model object.

**Broader support:** compare a 32-bit host trace with Ghidra guest-platform/context mappings against distinct mode-specific trace views. A flat host trace offers a common linear coordinate system and room for protected32 addresses, but platform/context mappings still need proof and cannot distinguish conflicting decode contexts at one address by themselves. Separate views make language choice explicit but fragment history/navigation and require careful linkage to the one live owner. The base language is fixed when `create_trace` runs; a mode property does not change it. Mode changes, remapped selectors/pages, and simultaneous 16/32-bit code require demonstrated Ghidra platform/disassembler support. Avoid one address space per possible segment or unbounded cached page maps.

Choose the advanced representation after a small protected-mode experiment demonstrates current-instruction decoding, data reads, far pointers and static mappings. A dedicated Java extension may supply DOS-aware location tracking, platform/disassembly context and mapping actions, but **no Ghidra fork, custom CPU language or Java descriptor emulator is assumed necessary or approved**. Keep translation authoritative in DOSBox, including query results the UI caches for an observed stop.

## 7. COM, MZ and analysis mapping requirements

Ghidra already has `ComLoader` and `MzLoader`. Reuse them rather than inventing executable importers. COM detection is heuristic; a tiny fixture may need explicit loader/language selection. MZ loading builds segments and processes relocations, so an imported project's addresses must be inspected rather than assumed zero-based.

### COM

The current COM loader places code at static `0000:0100`, with a PSP block below it. A normal COM runtime's code is at `PSP_segment:0100`. Map the observed loaded image range to the static code block using that actual load relation, not the current CS after arbitrary far execution. Distinguish a PSP block synthesized by the loader from bytes actually captured from the guest.

### MZ

Report the actual load segment, executable entry, PSP, image extent and load identity. Map code/data sections using the imported program's actual block/segment bases. Runtime relocation words differ legitimately from original file words; a naive full runtime-image hash must not reject a correct mapping or overwrite the analysis file with relocated bytes.

**Address mapping and relocation fixups are different.** Static mapping aligns ranges; it does not rewrite all far pointers in the static program or make static decompilation automatically depend on runtime DS. Keep file/loader provenance and the runtime relocation relation explicit. Map symbols/comments through Ghidra's existing mechanism, and use explicit user actions for copying dynamically generated code back into static analysis.

### Child programs, TSRs, overlays and self-modification

* Give each observed load its own lifetime/identity, even if its pathname or load address is reused.
* DOS child exit is not automatically the destruction of all guest state; TSR residency and shell/root lifetime remain native facts.
* Expire a module/section's old static mapping when an overlay replaces it. Do not keep a global address-to-file mapping forever.
* Runtime modifications remain trace bytes. Invalidate/re-disassemble affected dynamic code using Ghidra facilities; do not silently change the original static executable or its annotations.
* An unpacked/memory-only program can be copied to a new static program explicitly; its origin is a memory capture, not the original disk image.

Initial manual Map Module/Map Section operation is acceptable and useful. Automatic DOS-aware mapping can be added once load metadata and identity matching are correct. A matching filename alone is insufficient for automatic attachment to an analysis database.

## 8. DOSBox-Y source capability and gap analysis

| Required seam | Current source evidence | Integration work / limit |
|---|---|---|
| Exclusive controller | `native_debug_execution.h`, `native_control_owner.h`; Lua driver uses the same native operation/attachment | Reuse it. Ghidra selects the Lua frontend, not a passive second observer alongside another controller. |
| Execution | Native `Execution` has Pending/Active/Terminal, operation identity, retained result and release; managed Lua currently waits inside `Handoff::call` | Expose split start/observe/wait/Pause through the existing owner. Returning from an evaluator must not close the attachment or retire an active operation. Current Lua bindings do not yet provide this contract. |
| Stop coherence | Lua contexts carry native invocation/checkpoint; native results have start/end contexts. RPC separately exposes `state_revision` | Project session identity and a state-revision token from existing native context/lifetime, not a Python counter. Native validates each stopped read and registration. RPC's wire fields are prior art, not fields already present in Lua. |
| CPU snapshot | Lua response/encoder copies general and visible segment registers, flags, IP and `code_big`; RPC has its separate coarse `cpu_mode` snapshot | Native must report ordinary-real-mode admission, or the actual hidden base/limit/default-size/validity facts needed to establish it. `!cpu.pmode` also includes unreal mode. Refuse address-sensitive actions if admission is unestablished; the table-based/synthesized `ResolveSegmentedByte` is not evidence of effective CPU translation. Broader table/control projection remains advanced work. |
| Safe memory | Lua `read(context,address,length)` uses `PAGING_InspectLinear`; RPC separately supports segmented/linear/physical modes | Lua currently exposes a bounded linear read, not the RPC's entire address vocabulary. Reuse the nonmutating primitives within §6 limits; failed reads publish no successful bytes. |
| Selector lookup | `ResolveSegmentedByte` uses current `GDTDescriptorTable::InspectDescriptor` (which selects GDT/LDT using TI) | It is table-based, not an effective loaded-register-cache API. Do not claim it solves DS/CS cache semantics. Add a precise query at the native owner rather than replicate CPU translation in Python. |
| Breakpoint address | Lua breakpoint resolves CS:IP and calls `Debugger_BreakpointSet`; current driver restricts instrumentation to eligible stopped contexts and the normal core | Preserve those real limits. A flat Ghidra click needs qualified conversion; a source capability in another frontend is not automatic Lua support. |
| Watchpoint meaning | Lua watchpoint reaches existing MemoryChange machinery under build/core constraints | Not a general read/write/access trap set; later custom action only if its actual semantics are useful. |
| Callback events | `Handoff::call` pumps one native event on the Lua worker; `LocalDriverObserveHit` holds provisional read authority until verdict | Reuse serial event/wait ownership for synchronous wrappers. Ghidra R1 uses no Lua callback registrations or predicates (§9); never invoke Lua from the native thread. |
| Session transport | Managed session uses an inherited blocking stream FD3, bootstrap environment/limits and generic Lua `execute`/`close` records with strict generations | Live typed calls are adapter-owned constant Lua chunks plus data arguments, not a typed wire union. Add the narrow non-evaluating terminal dispatch required below. Keep sequential exchanges; no console forwarding. The Python adapter needs a thin non-MCP session client/launcher, not `AgentClient`. |
| Program/restore startup | `-lua-driver-session`, optional `-lua-driver-program` or fresh `-restore-state`, with stopped native readiness | Reuse the documented qualified entry path. Prove async binding operation at both roots; no program replay over a restored image. Full Ghidra restore UX remains R2b. |
| Register edits | Native `Debugger_SetRegister` exists; RPC list lacks a register-write method | Do not show target register editing as supported until bridged through the real owner. |
| Modules/regions | Neither inspected Lua bindings nor RPC list supplies the complete desired inventory | Export only necessary actual metadata; distinguish hardware inspectability from guest protection and code/data heuristics. |

These are **source-grounded integration gaps**, not a claim that current accepted DOSBox-Y behavior is broken. Historical RPC documents, Lua API names and Ghidra methods must not be treated as interchangeable callable interfaces.

### Existing asynchronous `dbg.live` seam: consolidate, do not add a rival API

The managed worker's `dosbox.run/step` bindings are blocking, but **Lua debugging as a whole is not synchronous**. The separate `dbg.live.resume/stop/status` surface in `debug_lua.cpp` already provides nonblocking control through machine-service slices. `Debugger_LiveResume` returns after selecting the normal loop; LiveStop selects a held loop; LivePhase enqueues a native `Action::Phase`, records `live_phase_operation` and yields the live dispatcher until settlement. `dosbox.cpp` services machine slices after ordinary loop/decoder frames return. Reuse this experience and the native machinery; do not invent a third async Lua execution authority.

These are not drop-in FD3 calls. The live surface runs its own nonrecursive emulator-thread dispatch slice/transport; the managed session evaluates on the dedicated Lua worker. `LivePhase` requires its yieldable live dispatcher, refuses managed evaluation, and retains a request line until completion. LiveResume is a raw-effect loop transition, not the selected core's exact-operation Run admission; LiveStatus does not yet expose the required complete operation contract. Calling it from the worker or spinning a synchronous wrapper inside `LiveSlice` would violate ownership/service assumptions.

**Consolidation decision:** factor one async native debugger operation/service projection from the existing `NativeDebug::Execution`, `local_driver_operation`, live phase/stop service and applicable stop-only queue seam. Bind the managed worker async API to it; migrate affected `dbg.live` execution entry points to that same core rather than keeping independent resume/phase coordination. Any retained public entry points are thin current bindings, not legacy execution paths or a second selected controller. The managed worker remains the one evaluator for the Ghidra session; its synchronous wrappers wait outside emulator-thread service slices. An implementation plan must identify this caller migration and preserve unrelated live input/capture functions without broad subsystem redesign.

One existing behavior must **not** be inherited: `Debugger_LivePhaseCompleted` sends Error, Breakpoint and ManualStop to `Debugger_LiveError`, whose `live_error` blocks later resume. Breakpoint/manual stops in the consolidated debugger must instead settle with their truthful reason and a valid continuable stop when native ownership permits it; only actual errors/lifetime loss deny continuation. This needs a focused native regression before reuse. Ordinary stopped breakpoint mutation also needs a supported shared binding, not reliance on the existing experimental live predicate gate.

The memory paths likewise have distinct semantics: legacy `Debugger_ReadMemory` calls ordinary `mem_readb`, whereas the managed worker's linear reader uses `PAGING_InspectLinear`. Ghidra observation must use the latter side-effect-free owner/primitives, with §6's translation limits. Consolidate duplicated implementation only where semantics truly match; do not silently change explicitly supported legacy read semantics or substitute the side-effectful legacy function merely because its Lua name is familiar. No frontend may implement its own CPU/MMU translation.

## 9. Proposed component architecture

```text
Ghidra 12.2 Debugger
  standard listings/registers/memory/breakpoints/modules/time
  required live-selection effect invocation guard
  optional narrow DOS-aware navigation/mapping extension
                  ⇅ Trace RMI 12.2
External Python DOSBox adapter
  ghidratrace client + schema + typed action registry
  stop/snapshot projection + register/address/static mapping adapters
                  ⇅ sequential managed-session exchanges (local FD3)
DOSBox-Y sole Lua worker: typed async debugger API
  start / observe / bounded wait / Pause / stopped reads / registrations
  synchronous scripting helpers = start + wait/event servicing
                  ⇅ existing worker/native handoff, evolved for async requests
                  ⇅
Existing exclusive native execution/memory/breakpoint owners
                  ⇅
DOS guest CPU, memory and hardware
```

### Owner-selected carrier and alternatives

| Architecture | Existing useful seams | Work and tradeoff |
|---|---|---|
| **Selected: Python Trace RMI → async managed Lua → native owner** | Existing initialized worker, qualified program-entry/fresh-restored stopped roots, native bindings, retained operation and event owners | Evolve blocking handoff/bindings; add explicit Pause and observation operations; implement thin session client/launcher. Adds Python process plus RMI and local session protocols, Lua/module dependencies and disposal responsibility. Removes guest-duration blocking from Ghidra without adding another CPU owner. |
| **Direct structured Agent RPC** | Typed observations, operation IDs/bounded wait, revision rechecks and a native stop-only service seam during Active; the RPC deferred-request gate can still delay Pause behind an outstanding ordinary request | The shortest evidenced local Ghidra path: Linux needs a thin client against existing UnixSocket support, while fresh-restored-root ingress is later work. It avoids the Lua refactor. The owner nevertheless firmly selects the managed Lua surface and its qualified startup/restore lifecycle for the long term, accepting greater initial work. RPC remains native-semantics evidence and an ordinary frontend, not a production or fallback integration carrier. |
| **Native C++ Trace RMI** | Direct access to native owner; no external Python adapter/session hop | Would need a new supported C++ client/binding of protobuf framing, negotiation, schema, methods and Ghidra version compatibility, plus socket work off the emulator thread. It could eliminate one process/protocol boundary but duplicates facilities provided by `ghidratrace` and couples Ghidra releases to DOSBox builds. Does not solve segmented analysis. Not selected. |

**Firm owner decision:** asynchronous managed Lua is the long-term debugger architecture, with synchronous wrappers above its one core and an external Trace RMI adapter. This is a strategic product decision, **not the shortest local Ghidra patch**, and accepts a larger Stage 0 refactor than RPC would require. The value is one reusable managed-debugger API over established native bindings and qualified program/restore lifetimes, not fewer protocol layers or lower initial cost. The bounded proof tests the proposed Lua design, not the carrier choice. Failure requires correcting/redesigning its native/worker seams or escalating a concrete impossibility; difficulty or a cheaper RPC path does not authorize switching carriers. Future functional specifications, technical design and staged implementation must retain this boundary.

`-lua-driver-session` expects inherited blocking stream FD3 and bootstrap environment/limits, not ordinary stdin or a listening socket. Its current private protocol is a **general Lua evaluator**: `execute` carries `input.code` and structured `input.args`; `close` ends serving. Calling Lua bindings does not make that wire a fixed-message debugger protocol. Reuse this framing/bootstrap for live exchanges; Stage 0 must extend its host/protocol narrowly for the non-evaluating terminal observe/release/close path below. Those terminal records are not supported today and must retain strict ordered exchange semantics, not introduce reconnect or generation recovery.

**Protocol ownership decision:** the extended record contract belongs to **DOSBox-Y's managed-session host and its client**, with one explicitly versioned DOSBox-owned schema/dispatch replacing the call to `lua_repl_mcp.session.serve` for this host. Reuse the pinned dependency's framing/bootstrap, codec and applicable evaluation/validation primitives as libraries; keep dependency pin `0b8d271911c17ce762246afa86b74ab20e999343` unchanged. Its strict `worker_protocol` decoder rejects the new records, so do not pass them through it or claim wire compatibility. Do not copy/fork that dependency's protocol implementation, maintain two DOSBox serving loops, or add format probing/fallback. Update affected DOSBox clients to the one host contract; the dependency's ordinary session consumers retain their existing contract. Preserve strict ordered generations, bounds and fatal ambiguous-exchange handling across both evaluator-backed and fixed records. Exact schema/module placement is Stage 0 technical design; this host-owned protocol is not implemented today.

**Selected trust boundary:** the local launcher/adapter is trusted and exclusively owns FD3 and the evaluator. Ghidra-facing Trace RMI exposes only a closed typed debugger method set, with no console/execute method or caller-supplied Lua. Each evaluator-backed live method selects an adapter-owned constant chunk calling a fixed Lua binding; parameters are separately encoded in `input.args`, schema/range-checked and passed as values. Never interpolate addresses, names, paths or other user data into Lua source, select functions via user-supplied names, or evaluate data with `load`/`eval`. Serialize all methods through the one client. Lua remains the implementation API; the private evaluator is not a sandbox against that trusted launcher or a compromised local process. The claim is **no Ghidra-facing general evaluator**, not that FD3 itself currently enforces a closed debugger vocabulary. A dedicated fixed-message live FD3 mode is not required for this trust model; terminal-only host dispatch is required to prevent new chunks after native departure, including a live execute racing that transition.

The thin non-MCP client supplies the ABI, extracts explicit results and owns disposal; reuse suitable components if available. No AI provider, MCP tool invocation or MCP server belongs in product semantics. Implementing the exact process/FD/bootstrap responsibility is real packaging work if a reusable client does not exist. Do not build a general remote-evaluation service. Opaque native contexts remain inside the Lua/native session; externally expose only copied identity tokens/data validated by their native owner.

Current `session.lua` requires the next `execution_generation`, advances it before evaluation and accumulates newline-framed bytes; it has no resynchronization or query-after-truncation operation. One full validated response with its matching generation establishes exchange completion. A native bounded-wait result of Active is such a completed exchange, not a transport timeout. Incomplete/malformed frames, mismatched generations or an exchange whose completion becomes unknown make the managed channel unusable: send no next generation, observe, retry or protocol close on it; the external owner disposes the child through its process lifetime and reports unconfirmed disposal if necessary. A complete typed result already received remains known even if subsequent delivery fails. No reconnect or new allocation generation recovers the old session.

To avoid making the client guess whether departure occurred between requests, use the new fixed observe/wait/release records in **both** live and terminal-only states: the host dispatches to the same native core while live and to the copied delivery slot after departure. Close retains its explicit close meaning. Other live methods use the fixed chunks/data arguments described above. A racing live execute is refused as a complete response without evaluating its chunk; a subsequent fixed observe is allowed only after that refusal exchange completes unambiguously. This is a narrow lifecycle protocol extension, not a second execution core or a claim that current `session.serve` already accepts those records.

The sole Lua worker serializes evaluation; FD3 can remain blocking and sequential because start/observe/Pause return promptly and each wait has a bound. Native guest execution continues through its ordinary owner after the start evaluator returns. Native service points must accept control observation/Pause while execution is active; simply returning early from `Handoff::call` is insufficient. Python only schedules short requests and projects results; it neither clocks the CPU nor runs a stepping loop. The Ghidra action guard is orthogonal to this carrier, not a replacement controller.

### Singular async debugger contract (proposed, not current function names)

| Primitive | Required meaning |
|---|---|
| `start(kind, stopped_context, options)` | Admit one Run/Step/Over/native run-until operation under the existing exclusive owner; return stable native operation identity, session identity and accepted/pending state, or a known refusal. Admission is not activation, instruction retirement or CPU progress. Fast completion may already yield a terminal result. Never wait for the guest to finish before returning. |
| `observe(operation)` | Nonblocking observation of that exact operation: pending, active, event-pending if applicable, or terminal. It neither starts execution nor consumes the terminal result. Active means native activation, not proof that instructions retired. Stale/unknown identity is explicit. |
| `wait(operation, bound)` | Bound the wait for native state/result/event availability; timeout means unresolved, not stopped or cancelled. Return the same operation's state without invoking arbitrary Lua callbacks. A synchronous wrapper explicitly services a returned event on the same worker under the rules below. The wait bound is not a hard real-time deadline over callback/foreign-code execution or transport failure. No unbounded guest wait is hidden inside the Ghidra path. |
| `pause(operation)` | Request native stop of the exact current operation through a serviceable native seam. Stale, unknown, released or retired operation identities are explicit refusals, consistent with observe/release; never target whichever operation later occupies the slot. The native owner checks exact attachment/session and operation identity when consuming the Pause, not merely when the adapter queues it, so a delayed Pause cannot affect a successor. If the exact operation already ended at a held stop and its result is retained, report that terminal fact. After departure, refuse Pause and direct the client to terminal observe. Pending is unconditionally **not pausable** in this contract: return an explicit Pending refusal without effect. There is no non-destructive Pending-cancel primitive; detach/owner departure are lifecycle changes, and checkpointChanged rejects Pending. Never force activation to service Pause or claim stopped before settlement. Only an operation observed Active by the native consumer can accept Pause; Pending may normally activate between exchanges, permitting a later fresh Pause. Acceptance is not proof of stopped state. Pause preserves the session and is not manager kill, worker close or process disposal. |
| terminal observation | One native settlement, carrying operation/session identity, terminal outcome stopped/exited/failed, truthful native stop reason and available end-context/state-revision token. Include register validity and continuation availability; failure/exit must not fabricate a usable held context. Repeated observations may return the same terminal fact; they are not repeated settlements. |
| stopped read / registration / removal | Native checks current exclusive owner and exact stopped context at use. Return copied data or explicit refusal. Arbitrary observation while running is not enabled by async control. Callback event reads use only their separate provisional context. |
| terminal retirement | At an ordinary held stop, retain the native result until exact-operation release; only then can another execution occupy the slot. On native departure, transfer the copied terminal facts to the session's sole terminal-delivery slot before retiring the native slot; subsequent exact-operation release consumes that delivery slot and closes the departed session (§9 below). Observing either representation is nondestructive. No result catalog or replay protocol. Lost result/release responses retain uncertainty, never authorize replay. |

Use existing native operation serial, attachment/session lifetime and invocation/checkpoint as authority. The wire-level state-revision token must represent that native state, not a new Python/Lua counter that claims to certify it. There is one current operation and one retained terminal result, matching `NativeDebug::Execution`; on departure its copied delivery representation replaces live native retention, not a second authoritative operation record. No unbounded operation catalog. The precise JSON/token shape is an implementation detail to settle in the proof, not an already supported RPC schema.

The source separates admission (`enqueue` → Pending), activation (`activate` → Active), settlement (`settle` → Terminal) and release. Current `LocalDriver` waits to copy and release the result before completing the blocking handoff; the async design must separate those points while preserving native lifetime and terminal-flush ordering. This is real native/worker work. Merely putting the old synchronous call on a Python thread would still occupy the evaluator/FD3 request and would not implement this contract.

### Stage 0 refactor boundary

1. **Split handoff exchange from operation lifetime.** Return the request channel to availability after admission while `local_driver_operation` still names the one native operation. Permit short control/observation exchanges during Active. Preserve one retained terminal result keyed by that native identity, not an Agent-style map or a second Lua operation state machine. At departure, use the terminal-only delivery ordering below; the old rule that only an outstanding blocking evaluator may flush cannot survive unchanged. The copied response is a projection of the native result, not independent execution authority.
2. **Factor the active stop-only service seam into shared native debugger code.** `LocalDriverFrontier` currently dispatches Run and returns (around `debug.cpp:6370–6377`); there is no Lua request consumer loop left servicing Pause during that Run. `EmulationThreadQueue::Pump` already admits stop-only work while Active without reordering deferred ordinary effects, but its implementation and current pump call sites are `C_DOSBOX_AGENT`-gated. The documented managed-Lua build does not require that flag. Factor the minimal stop-only service primitive and required call sites into native debugger code available under the selected managed-Lua build independently of Agent/RPC, with both frontends using the shared primitive and preserving the selected attachment. Merely calling the Agent-gated queue or requiring Agent to be enabled is not the remedy; do not call the Agent controller or copy its coordination into a parallel Lua engine. The current queue refuses all work while Pending, so its existence does not establish pre-activation cancellation. Read/status projection must likewise be safe across threads and remain observation-only, not an ordinary guest read admitted while running.
   **Active service point is required, not already available:** `LocalDriverFrontier:6108–6112` drops Predispatch/PostTick for a condition-free resume absent a terminal result. Refactor that path to service pending control/state requests on the emulator thread even during unrestricted async Lua Run. Factor submitter, command consumption and native Pause body, not only a queue type. Use an existing safe native seam and a minimal pending-control predicate; qualify its hot-loop cost rather than promising zero overhead or per-instruction latency. Preserve ordinary CPU dispatch and no guest reads while Active. Pending state observation/refusal must complete without activating the guest; do not copy the Agent pump's Pending no-op or break-after-execution rules blindly into the new control path. Exact service placement/synchronization is technical-design work. No second CPU loop, synthetic repeated phases, Agent flag or added native stop condition may be used to make the condition-free continuous-loop proof pass.
3. **Redesign provisional-event lifetime independently of an executing evaluator.** `offerEvent` requires `Phase::Executing`; `LocalDriverObserveHit` currently owns read authority in the hit stack until verdict. Freeing the admission evaluator invalidates the first assumption, and unwinding that frame invalidates the second. Preserve serial callback semantics through a native-owned provisional-event lifetime and independent event-service exchange, not by retaining the existing handoff unchanged. The initial proof rejects callback-bearing admission; see the migration requirement below.
4. **Implement the external session client/lifecycle boundary.** The Python adapter must create the socket pair/inherited descriptor and bootstrap values, use existing framing and result encoding, serialize constant-code/data-argument live requests, bound waits and verify disposal. Extend terminal dispatch/lifetime narrowly as specified below; the current generic session evaluator and stopping predicate do not already supply it. Reuse a suitable client/manager where available; otherwise this is explicit additional work. Keep it local and non-MCP with no Ghidra-facing generalized evaluator. Treat ambiguous FD3 exchanges as terminal channel failure, not a reason to send a status query. Lua worker protocol close alone does not prove emulator exit.
5. **Deliver the DOSBox-owned protocol artifact and its documentation together.** Stage 0 owns the versioned host schema/dispatch source replacing this host's `session.serve` call, all affected DOSBox clients, focused protocol/lifecycle tests, and `docs/managed-lua-session.md` updates describing the DOSBox protocol identity, library reuse, ordered generations, non-evaluating records and replacement of the old “no next chunk” lifetime rule. Record the actual DOSBox host/client release identity and update applicable package metadata and third-party notices/distribution documentation for the shipped composition. Preserve the unchanged dependency's existing repository/commit/tree/archive identity and notices as dependency provenance; do not relabel those bytes, imply a dependency revision, or patch the installed pinned module. The host owns dispatch from initial serving through terminal delivery, not a second loop grafted on after dependency `host_stopping`. Stage 0 is incomplete until this coherent artifact/client/docs/test/package work and the bounded departure proofs are delivered; R1 packages that same qualified contract.

These changes may require a coherent native/worker refactor. They do not authorize replacing the native facade, inventing operation clocks/registries, or reimplementing Agent admission/wait/Pause semantics in Python. The precise factored seam is a Stage 0 design result to validate against source, not a claim that the current Agent queue can be attached unchanged to the Lua driver.

### Native departure: terminal-only delivery on the same FD3

**Selected rule:** natural selected-program exit, fresh-restored root exit or another known native terminal departure ends guest authority immediately, but does not immediately end the managed session's delivery channel. Keep the same child process, sole worker and FD3 alive **only to deliver the one copied terminal result** until exact release, explicit close or external disposal. No new child, reconnect, attachment resurrection or guest re-admission. Process lifetime may therefore exceed guest lifetime. A launcher-observed process exit/EOF is a separate host fact, never a substitute native outcome.

This changes an existing contract deliberately. `LocalDriverFrontier`'s ExitFence currently settles/releases the native result, calls `handoff.terminal()`, completes an outstanding Executing response and waits for Lua closure. Restored-root close follows a similar path. The current `terminal()` sets closing, `complete()` requires Executing, and session serving allows only the current evaluator's flush, with no next chunk. After async admission returns, none of that provides a later observe window. `sessionRead` may be idle in blocking `recv`; the current endpoint `wake()` shuts down FD3 in both directions and cannot be reused to wake it while preserving result delivery.

The new ordering is:

1. **Native departure/settlement.** At the actual native departure seam, irrevocably revoke guest effects and all stopped/provisional borrowed contexts for this session. Settle the selected operation once through the existing native owner. Copy its exact operation/session identity, actual native outcome/stop reason, representable final state-revision identity and available exit facts into the session-owned single terminal-delivery slot before releasing native storage. A copied end-context token is historical provenance only: `continuation=false`; absent registers/revision are explicitly unavailable rather than fabricated. If no operation remains selected, publish only a session-departure fact with no invented operation ID. A partial/fatal teardown that cannot produce known native facts remains unknown.
2. **Publish terminal-only mode and wake.** Serialize publication against session dispatch so no subsequent request can regain live admission. Wake both a bounded-wait consumer and an idle FD3 reader via a **local non-destructive wake source** serviced by the session read loop (for example an event descriptor alongside FD3); do not inject unsolicited bytes into response framing, use transport EOF as a notification, or call `shutdown(SHUT_RDWR)` at this transition. The single reader can wait on FD3 plus that wake source; no second evaluator/thread or polling clock is needed. After noticing terminal mode, it continues to service permitted requests on FD3.
3. **Retire native authority without waiting for the client.** Release the exact native result once after its delivery copy is owned, clean up native registrations/attachment under their existing owners, and let guest/native frames unwind. Move the session-lifetime wait/join out of the borrowed ExitFence/guest frame: a top-level managed-session owner retains only copied facts, the worker and transport while native teardown completes. It must not join a worker which is waiting for another native response. Any in-flight observation/event is refused/settled through the existing closure/verdict rules before its provisional context disappears. No live CPU frame is retained to answer terminal requests.
4. **Restricted delivery.** In terminal-only mode permit only exact-operation terminal observe (including wait as immediate terminal observe), release and close. They operate solely on the copy; repeated observe returns the same facts without new settlement. Refuse Run/Step/Pause, guest reads, registration changes, callbacks and arbitrary new Lua evaluation chunks before evaluation/native dispatch. This requires a narrow typed terminal dispatch path in the session protocol/host, not merely allowing generic `execute` while native bindings fail. An already-running synchronous wrapper may finish consuming its terminal and flush its current response, with guest admission revoked; no new general evaluator chunk follows. Host Lua finalization is not guest authority.
5. **Release/close and process completion.** On the exact terminal release, consume the copied slot and send/flush the release response on FD3 before normal worker close; a mismatched ID cannot consume it. Explicit close may discard the unconsumed copy, with that loss acknowledged; peer EOF or manager disposal may lose it without a receipt. Close wakes blocked reads, ends session serving, closes Lua, and publishes worker-closed. Only then does the managed-session owner join the worker and close its owned FD3/wake descriptors, completing ordinary child exit. Lua finalizers cannot regain native effects; arbitrary foreign-code blocking still has no cooperative preemption guarantee. A stalled/lost peer is contained by the external process owner and, if necessary, destructive disposal, not by keeping a guest frame or replaying work.

For an ordinary held terminal, exact release frees the live native slot and allows later execution. For a **departed** terminal, native slot release already happened during transfer; the same public exact-operation release consumes the remaining copied delivery slot and closes the session, never calls the retired native owner or allows another Run. These two lifecycle cases are explicit, not independent execution paths. The copy remains the original native terminal evidence; the adapter cannot synthesize a guest reason from exit status. If FD3/process failure prevents receipt, report host exit/disposal separately and native outcome unknown unless previously received.

The top-level delivery owner may wait for the client while retaining only transport/worker/copied data. The native owner never waits for remote release inside ExitFence, and terminal requests never wait for a native seam that has departed. External disposal remains possible if the client stalls; there is no automatic delivery timeout, replay, durable result journal or unbounded retention catalog. Exact API/host changes are Stage 0 implementation work, not a claim that current teardown can simply be reused unchanged.

If departure has no selected operation, terminal observe may address the session identity to retrieve only that departure fact; close ends its delivery window. No fabricated operation or operation-release receipt is created. A retained operation result remains addressable only by its exact operation identity.

#### Required interaction traces

**Exit between requests:** `start(O)` → admitted response completes → Python is idle → selected COM exits → native settles O, transfers its terminal, revokes contexts and retires → terminal-only worker is woken without closing FD3 → `observe(O)` returns copied exited/reason/revision facts → second `observe(O)` returns identical facts → `release(O)` reply flushes → worker closes, owner joins/closes FD3, child exits. No next guest context or shell decoder is admitted. Early EOF instead of the typed response is a failed delivery, not successful Run completion.

**Exit immediately after a bounded wait:** `wait(O, short)` returns Active → restored root exits before the next request → the same terminal transfer/retirement occurs → the next `wait(O, short)` returns the copied terminal immediately, without entering an evaluator that needs a live guest seam → adapter records the actual root-exit facts and releases O → normal close/disposal ordering. If exit races the first response, that response may truthfully be Active or terminal according to its observation point; all later permitted observation sees the single terminal. A subsequent guest read or Pause is refused, not redirected to a restored shell or new owner.

### Synchronous convenience and callback constraints

Synchronous `run`/`step`/`over` helpers do exactly **start → bounded wait/event servicing until terminal → copy result → release**. They intentionally block their scripting caller and return the same terminal semantics as the async primitives. A wait deadline is not an implicit Pause unless the caller explicitly requests that operation. Interactive Ghidra never invokes these helpers: it returns from the start action, then issues short observe/wait calls interleaved with Pause. Long native execution is not assigned an artificial timeout merely to make the UI responsive.

There is source support for serial callback pumping, but not yet for the proposed split API. `Handoff::call` already waits for Result or Event, invokes its supplied observer on the Lua worker with the handoff mutex released, and returns a verdict. `lua_driver_worker.cpp::observation` uses protected Lua invocation and existing cooperative budgets; `invoke` admits only Read/TextSnapshot during observation. `LocalDriverObserveHit` holds the native provisional event and services exact-context observer reads until it consumes the verdict. This is a plausible shared wait/event implementation for wrappers; it is not evidence that callbacks can run while arbitrary session evaluation is free.

Preserve callback **behavior**, not the current handoff state machine. The one core must own at most one exact provisional event tied to operation, registration and native lifetime, independently of whether a worker call is Executing. Native execution remains held for that event until the sole worker's bounded event-service request invokes the callback and its exact verdict is consumed. Callback references remain valid through settlement/removal. Ordinary stopped authority is not granted. The owner must keep the real hit/continuation authority valid for any provisional reads; copying an image/token does not extend a stack borrow. Technical design must specify how the native event lifetime holds or safely suspends that hit continuation and expires authority on verdict, close, error or departure, before the underlying frame can disappear. This is an explicit native ownership redesign, not permission to leave a dangling stack context or to re-enter native execution from Lua. No unbounded event registry or parallel callback dispatcher.

Only the sole worker in explicit event service invokes Lua; plain observe/wait returns event-pending without invoking callbacks. No native-thread Lua, concurrent evaluator, recursive Run/Step or guest effects from predicates. Bounded native waiting/closure arbitration must settle an unserved event as an explicit error/stop rather than strand its owner. An expired event cannot later accept a verdict or read. Existing error/budget/close verdict semantics remain stop-producing; departure first invalidates event authority and settles/refuses the pending service before native teardown, after which only copied terminal delivery survives. A callback-bearing synchronous wrapper must use this same core and event service, not the old blocking engine. Cooperative callback budgets still cannot preempt arbitrary blocking foreign C; external disposal is the final containment mechanism, not a hard callback deadline.

**R1 externally driven async profile:** no Lua predicates or callback-bearing breakpoint/watchpoint registrations may be armed when Ghidra starts execution. Use native stop conditions and ordinary native breakpoints, and return typed stop observations on request. Refuse unsupported callback-bearing admission explicitly; do not silently discard existing callbacks. The first Stage 0 experiment may be entirely callback-free. Before replacing current synchronous scripting behavior, prove one representative callback on the shared wait/event path. If that cannot yet be done cleanly on one async core, report the affected current callback contract and stage its migration explicitly; do not retain a hidden old execution engine or claim callback preservation. Removing an existing supported callback capability needs a scoped owner decision, not inference from the carrier choice. Exposing callback-bearing external async scripting later requires an explicit service obligation for pending events; it is not necessary for Ghidra R1. A pending native event must never be mistaken for an ordinary stopped revision or abandoned on close. Shared-core callback preservation is a migration requirement/proof obligation, not a completed capability.

### Earliest bounded architectural proof

Before constructing the full Ghidra model, use the actual managed session and a small ordinary real-mode loop: async Run returns an admitted operation promptly; **the same sequential FD3 channel** observes status and requests Pause; that exact operation settles once, and its stopped revision admits register/memory reads. Observe the terminal result twice without duplicating settlement, release it, and prove an old identity cannot control the next operation. Repeat the corresponding scripted run-to-stop using the synchronous wrapper and compare relevant terminal outcome/reason/context meaning, not numeric identities across runs. After the callback-free proof, exercise one callback through the shared wait path before claiming preservation of that existing scripting contract; verify explicit rejection in the external callback-free profile.

Run that continuous-loop proof with `C_DOSBOX_AGENT` disabled and **no stop conditions**; verify the new emulator-thread Active service point consumes Pause. Check Pending Pause refusal at native consumption without forcing activation. Callback preservation additionally exercises close with a provisional event pending on the redesigned event lifetime; no old execution path may supply the passing result.

Also initialize one fresh restored stopped root through the established startup path, inspect without advancing the guest, then explicitly execute; verify disconnect/worker-close/process-disposal distinctions using the managed child. This is a small native/session proof, not a Ghidra restore product milestone or an exhaustive archive matrix. If the current handoff cannot serve Pause while active or safely preserve event/result ownership after evaluator return, revise that precise native seam before proceeding. Do not hide failure behind a second controller, Python worker pool or timeout-kill masquerading as Pause.

Explicitly exercise both departure races described above: **exit between FD3 requests**, and **exit immediately after a bounded wait has returned Active**, covering selected COM and fresh-restored root departure. Require typed terminal receipt twice, exact release, refusal of post-departure guest reads/effects, no blocked native teardown awaiting a vanished evaluator, and normal process closure only after the delivery window closes. Separately disconnect during that window: report lost delivery honestly and confirm disposal without manufacturing a native reason. Also exercise breakpoint/manual-stop completion followed by another valid operation through the consolidated live/worker core; a latched error on an ordinary hit fails the proof. These cases test the chosen lifetime/consolidation rules, not a new exhaustive matrix or pipelined FD3 protocol.

The required historical-action guard and any further justified navigation extension belong on the **Ghidra side**. Accurate caches/translation results belong on the **DOSBox side**. Ghidra remains responsible for its analysis/navigation projections, DOSBox for machine execution and effective memory semantics. There is no requirement to change the Trace RMI wire protocol to send segment tables.

### Why not the alternatives?

* **GDB stub:** valuable for broader interoperability, but it is another protocol and still needs segmentation/module mapping. Not the shortest Ghidra-first route.
* **ret-sync:** useful for synchronizing static listings; not a replacement for full live registers/memory/time/actions integration.
* **Direct Java socket client:** possible but duplicates the integration path Trace RMI already supports and couples transport to GUI code.
* **Embedding Ghidra/JVM or Python inside DOSBox:** unnecessary coupling, packaging and lifetime complexity.
* **Existing DOSBox Ghidra prototype:** `yetmorecode/ghidra-dosbox-agent` is prior art using the older Java debugger model, not a ready 12.2 Trace RMI backend. Earlier inspected memory access was placeholder code and no explicit license was identified; do not copy it without fresh functional/license review.

## 10. Execution, events and failure semantics

1. **One authority.** The managed Lua/native session owns the one exclusive attachment; Python owns its external process/session lifecycle. Ghidra UI focus changes grant neither a new attachment nor permission to invoke another frontend.
2. **Admission is not progress.** Start returns a native operation selection in pending/admitted state. Show pending status explicitly; native Active can be projected as RUNNING without claiming instruction retirement. Only the same operation's terminal observation proves its final outcome. No new acknowledgement mechanism is required.
3. **Control remains responsive.** Use nonblocking observe or short bounded waits on the one sequential FD3 channel, with exactly one exchange in flight. During Run, Interrupt can wait behind that one short observation/wait; do not place a GUI refresh backlog ahead of it or hold the evaluator for Run's duration. The new native Active service point, not another Python CPU loop, implements interruptibility. A bound on native state waiting is not a hard real-time guarantee over blocking foreign code or an unresponsive transport.
4. **One stopped snapshot.** Once native stop is confirmed, capture registers, mode/cache metadata and requested memory for that same session/revision. Evolved Lua responses must project the native context/lifetime identity; existing RPC checks are useful prior art, not an already implemented Lua guarantee. Serialize observation batches against effects in the adapter, compare returned identities, and publish only that stop's observations. No new clock or currentness registry. Stale observations are not combined with the next stop. Later reads enrich only the still-current stopped snapshot; historical UI selection never redirects live reads to the past.
5. **Truthful reasons.** Preserve raw native reason plus an explicit projection for startup/entry, step completion, breakpoint, requested Pause/manual stop, explicit stop-condition match, budget exhaustion, cancellation, program/root exit, fault/error and owner/attachment/continuation loss. These are not interchangeable: budget is not necessarily guest elapsed time, startup is not a step, and a wait timeout is unresolved rather than any stop. New/unmapped reasons use `unclassified` with their raw value, never guessed success/failure. Apply the selected Lua/native taxonomy, not a blindly copied RPC enum. Program-exit observation, a request to dispose, and confirmed host-process disposal are separate facts; when causality is not established, say target ended and cause unknown rather than label it natural or requested. Existing RPC stop handling collapses these distinctions and is not a sufficient model for causal claims.
6. **No replay of effects.** After a transport timeout/disconnect, do not automatically resend Step, Continue, writes, breakpoint creation, Save or Restore. Trace publication failures also do not undo known native effects.
7. **Historical selection is observation only.** Browsing an old snapshot may update GUI focus, but cannot cause native resume/write/restore. Enforce this with §5's required Ghidra-side invocation guard and focus-only `activate`, not backend validation alone: standard effect methods carry no selected snap. Backend admission separately checks the actual current target/stop/operation.
8. **No hidden fallback execution.** A failed read or unavailable capability cannot be “fixed” by stepping, triggering an interrupt, loading a selector, invoking a guest DOS service or rereading MMIO.
9. **Disconnect policy must be explicit.** R1 owns a disposable emulator child: loss of Ghidra/adapter connection ends that live debugging session and the launcher/owner contains and disposes the child through supported process/native lifetimes. Ordinary Pause keeps the session usable. Session-worker close and destructive manager kill/process disposal are different, and a closed FD alone proves neither guest stop nor process exit. Process termination is not rollback or proof of Lua finalization. Closing/saving a trace may preserve its database while ending its live target. Nondestructive attach/detach to pre-existing instances is later work. If disposal fails, report the unconfirmed child.
10. **Transactions stop at the trace.** A failed publication may leave an incomplete observation snapshot; mark it as such and preserve effect uncertainty. Do not build exactly-once/recovery machinery to pretend the guest and Ghidra commit atomically.

### Observation failures are not effect uncertainty

| Outcome | Meaning and frontend response |
|---|---|
| Known refusal before effect admission | Report unsupported/running/stale/invalid input as appropriate; no guest effect is claimed. A corrected user command is a new operation, not recovery. |
| Error reply after admitted effects or failed readback/reporting | Failure does not prove no effect. Preserve known effects and uncertainty; never replay merely because the reply is an error. Existing Agent `WriteMemory` writes then calls `ReadMemory` for readback, demonstrating the distinction. R1 exposes no writes; any later edit surface needs typed effect/reporting facts, not assumed equivalence of safe inspection and writability. |
| Failed **inspection-only** read with a complete validated response | No guest mutation was requested. Mark the requested current range unavailable/unknown and allow a later fresh read over the still-unambiguous channel; an ordinary typed read failure does not poison the session. |
| Native operation accepted and ID received in a full exchange; bounded wait returns Pending/Active in another full exchange | Still unresolved. Later observe/wait of that same identity is allowed on the healthy channel; do not issue another Step or Run. A returned native wait timeout is not a missing transport response. |
| Truncated/malformed request or response, generation mismatch, or exchange completion unknown (including transport timeout) | Channel failure is terminal even for an inspection request. No subsequent observe, next generation, resend, protocol close or reconnect on that channel. Dispose the managed child through the external process owner; preserve known facts and uncertainty about admitted effects, and report unconfirmed disposal when applicable. An operation ID alone cannot repair ambiguous framing. A complete result later lost only in Trace publication is the separate case below. |
| Effect known complete; trace publication failed | Report the known effect and missing projection. Fresh stopped observations may repair the display, but neither an aborted trace transaction nor an empty UI proves the effect did not happen. |

This permits ordinary no-effect observation and known-operation polling only after known-completed managed exchanges. Fatal channel handling follows this protocol's framing/generation constraints, not another product's persistence policy; no exactly-once promise follows. RMI effects require both §5's live-selection invocation guard and current native capability/admission. Backend current-target checks alone cannot enforce historical-view exclusion.

## 11. Save states, Ghidra traces and persistence

Three different things must remain separate:

| Artifact | Meaning |
|---|---|
| Ghidra static Program | Imported/created analysis database: symbols, types, code, comments and original/static bytes |
| Ghidra Trace snapshot | Captured observations at a debugger time; may contain only selected memory |
| DOSBox save archive | Native guest machine state accepted by the existing compatible fresh-target restore contract |

Selecting a Trace snapshot is not reversing DOSBox. Ghidra's p-code emulation is also a separate analysis facility, not DOSBox hardware execution. Do not advertise reverse stepping through a DOSBox state timeline unless separately designed and tested.

For initial restored debugging, use a **new trace per fresh restored process**. Include available save identity and native target provenance as metadata, not invented historical debugger events. Reapply user-selected breakpoint intent only through fresh supported operations; an old native breakpoint handle is not a live handle in the new instance. Do not serialize Lua, Python agent or Ghidra interpreter state into the machine archive.

Any later “Restore in Ghidra” action must visibly create/adopt a new disposable target and trace, not mutate the used destination or silently replay a guest launch. Save uses the existing owner and result semantics; it is not made safe merely by invoking it through RMI.

## 12. Packaging, deployment and practical performance

* Baseline UI: released **Ghidra 12.2**, whose README requires **64-bit JDK 25**. Use its documented Python/runtime support; `ghidratrace` declares Python `>=3.9` and `protobuf>=6.31.0`. Prefer an isolated environment with release-matched dependencies; do not mutate Ghidra's installation opportunistically.
* Ship an external agent package, minimal schema, launcher/help and the required version-matched Ghidra historical-action guard. Additional mapping/navigation extensions remain justified only by consumer gaps. Launcher metadata must integrate with the existing Trace RMI launcher menu. Do not vendor an entire GDB agent and leave irrelevant features enabled, especially its console execution method.
* The selected backend requires the managed Lua build and matching Lua module/ABI, in addition to native debugging. Stage 0 must qualify the factored native stop-only service in that build with `C_DOSBOX_AGENT` disabled; enabling Agent/RPC is not an integration prerequisite. Reuse the unchanged pinned dependency as libraries with its original identity/notices; the versioned DOSBox-owned host protocol, clients, docs, tests and applicable package metadata/notices are explicitly Stage 0 deliverables (§9 item 5), not a dependency release or reuse of its unchanged serving loop. Do not make MCP or an AI provider a runtime requirement. Heavy-debug/watchpoint and normal-core restrictions must be reflected accurately. Packaging must supply the FD3/bootstrap client/launcher and confirm disposal, not merely install Python modules.
* Start on the project's qualified Linux/SDL2 managed-session build; qualify other platforms separately, including the inherited session-transport/bootstrap path. Windows named-pipe support in the alternative Agent client is not evidence for this Lua backend. Do not infer cross-platform support from Ghidra's own platform list.
* Use loopback/local IPC by default. Trace RMI remote methods are control authority; TCP connectivity is not authentication. Remote access requires an explicit protected transport/deployment decision, not a public unauthenticated listener.
* Keep executable/output paths, guest images and any credentials out of accidental verbose trace/log publication. DOS memory captures can themselves contain sensitive data; saving/sharing a trace is an intentional user action.
* Use on-demand memory/table refresh and bounded transfers. Capture PC bytes, essential registers and selected stack/data at stop, not all guest RAM and every descriptor after every instruction. Ordinary UI inspection must remain responsive.
* Batch RMI trace publication, not guest execution effects. Preserve request order, native identity, cancellation and stopped-observation semantics under UI refresh bursts.
* Retain third-party license notices. Ghidra/ghidratrace are Apache-2.0 components; DOSBox-Y is GPL-derived. Process separation is an architectural choice, not a legal compatibility conclusion. Existing dependency/distribution decisions remain separate.

## 13. Staged implementation and acceptance proposal

These are proposed future deliveries, not authorization to implement. Package, document and independently assess each delivered capability on its actual combined input. Installation is part of every release, not a milestone deferred until protected mode works.

Stage 0 is the shared async-core/carrier refactor and artifact qualification prerequisite to R1, the first usable COM integration. R2a adds MZ usability, R2b adds the Ghidra fresh-restore workflow, and R3 covers later advanced contexts. These delivery labels are independent of this analysis's author revision numbers.

### R1 — ship a useful real-mode COM debugger

**First prove the carrier:** §9's small actual async/session test must pass before committing to the complete adapter. It proves responsive same-FD3 control, native operation/result ownership, synchronous-wrapper equivalence and fresh stopped-root startup; it does not require implementing all Ghidra views or a restore launcher.

**Stage 0 artifact gate:** deliver §9 item 5's single versioned DOSBox host protocol source, affected clients, focused tests, updated managed-session documentation and applicable packaging/notices before claiming this carrier milestone complete. R1 must ship those same artifacts; the unchanged dependency pin is library provenance, not the new wire-contract identity.

**Then prove the Ghidra consumer:** one actual Ghidra 12.2 window connected through the selected async Lua carrier to a stopped COM at a non-`0x1000`-aligned load segment. Show actual register values, native bytes, frame-PC tracking, explicit SS:SP navigation and a manually selected static mapping. Exercise §6.3's near-indirect/relative/far branch cases and distinguish bytes/PC from reference/p-code correctness. Decide a truthful initial analysis limitation versus a narrow consumer-side remedy. A constructed trace alone does not close this proof.

**Implementation scope:** evolve the Lua/native handoff to the singular async contract and synchronous wrappers; factor the Agent-independent Active control service; provide the trusted constant-code/data-argument session client and non-evaluating terminal dispatch; add minimal native mode/load metadata; implement typed RMI schema/actions, exact native hit identity/reason projection, range-limited safe reads and execute-breakpoint conversion; ship the Ghidra-side historical invocation guard, managed child lifetime and help. Update affected current Lua callers coherently; preserving synchronous callbacks requires the native provisional-event lifetime redesign, not unchanged handoff reuse. No simultaneous old/new execution paths, production RPC carrier or generic carrier framework. COM provenance can initially be explicit user selection plus observed entry/load metadata; automatic module discovery is unnecessary.

**One coherent acceptance scenario:** clean install → import known COM → launch stopped → map its static code → set a static-listing breakpoint → Continue to the actual hit → Step Into/Over a known call/write → refresh changed bytes → Interrupt a continuous loop → exit. Include a far transfer and segment alias. Capture a page at stop 1, modify it through execution, and reach stop 2 without recapturing it: old bytes must not become current-known merely through snapshot inheritance. Refresh with history selected must not write current bytes into the old snapshot. Repeated stopped GUI reads must leave representative registers, guest progress/time and an applicable device-side-effect sentinel unchanged; a refused device range must not call its read handler. Use a focused native observation test for the latter if the fixture cannot expose it directly. Do not replace this with whole-memory hashes or exhaustive device sweeps.

**Small failure/lifetime checks:** a real competing frontend/native attachment attempt is refused; this is distinct from a second transport connection timing out. FD3 has one inherited peer, not a public multi-client listener. Read while running is refused without a hidden stop; unsupported mode/high-memory observation is honest; one dropped effect response is not replayed; disconnect disposes the managed child or reports failure. During Run, schedule only short control/status/wait requests, not deferred stopped-memory refreshes; consume each reply before the next request. Interrupt must remain serviceable with a queued GUI refresh burst, but cannot overtake bytes already in the single in-flight exchange or promise idle latency during blocking native/foreign code. Native breakpoint/manual-stop settlement must permit the next valid operation rather than inherit the live phase error latch. Successful R1 may ship without MZ, restore, protected mode or target writes.

Include §5's historical-effect and two-breakpoint/internal-step attribution checks. Verify no Ghidra console or generic command path reaches the evaluator, and metacharacters/quotes in data remain values rather than Lua source. In a bounded channel-failure fixture, truncate an effect response: no next-generation observe is emitted and external disposal is confirmed or explicitly unconfirmed. Contrast a complete Active wait response, after which observation is allowed. These are targeted contract checks, not a transport-fuzzing matrix.

### R2a — MZ and reusable analysis

Add actual MZ load/section/relocation provenance and mapping UI improvements. A fixture with distinct code/data/stack segments, near/far calls and relocations is loaded at two bases and correlated to the **same** static Program. Show matching symbols/comments and truthful dynamic bytes without overwriting original analysis or mistaking relocation changes for a wrong executable. One runtime code modification demonstrates re-disassembly/freshness handling. Static decompilation assumptions and any unqualified dynamic watches remain explicit.

### R2b — fresh restore

Build the Ghidra restore workflow on the managed Lua fresh-root ingress already exercised by the carrier proof: select a compatible archive/profile, create a fresh disposable process and new trace, inspect saved PC/registers/selected bytes before any explicit execution, then continue to a known result. Do not combine program replay with restore. The earlier native/session proof is reusable evidence, not proof of this complete UI workflow. Old native handles and historical UI objects cannot address the new process. One rejected archive/profile proves truthful failure and disposal. An in-Ghidra Save action can follow independently; existing external saves suffice for this restore delivery. Lua, Python and Ghidra execution stacks are never machine-save payloads.

### R3 — later increments: DOS lifetimes and protected/mixed mode

Child EXEC/return, TSR residency, overlays and address reuse can be separate bounded increments with mapping-lifespan evidence. They do not require artificial host process/thread models. Withdraw obsolete mappings when their load identity ends; retain genuinely resident code when evidenced.

Before a protected-mode implementation, run a small representation proof comparing §6.5's flat host/platform approach with mode-specific views. Demonstrate both Ghidra decoding and native-backed address conversion, not merely descriptor objects. Then ship separately qualified profiles. The relevant compact fixture set covers changed descriptor versus loaded cache, unequal caches for equal visible selectors, 16/32-bit defaults, a far pointer, an invalid selector, nonidentity paging, table/TLB divergence where applicable, and A20-effective versus backing-memory meaning. Select cases needed by the advertised profile; do not require all extenders, all devices or every mode before shipping one correct profile. Reuse the existing native CPU/MMU owners.

### Validation budget

Use a few meaningful actual consumer-path checks and focused conversion/ownership regressions. Preserve failed observations and explain what they limit. Reuse unchanged native evidence instead of rerunning broad suites or repeatedly hashing installations. No Ghidra runtime result, performance number or delivery estimate is claimed by this analysis.

## 14. Acceptance boundaries and unresolved questions

### Required before claiming a usable initial integration

* A live Ghidra UI, actual DOSBox backend and a real DOS fixture—not only a constructed trace or mocked server.
* Correct PC/segment registers, memory bytes, breakpoint mapping and stop/exit states.
* No inspection-induced guest execution or device effects.
* Explicit compatibility/build capabilities and clean owner/process lifetimes.
* Static/dynamic mapping that preserves original annotations and handles the advertised load relation (COM in R1; MZ relocations in R2a).
* Errors that distinguish unsupported/unreadable/stale/running/exit/uncertain effects.

### Not established by this research

No Ghidra UI was launched, no new Trace RMI connection was exercised, and no implementation was written. The evolved async Lua handoff, Agent-independent Active Pause service, typed lifecycle dispatch, terminal retention/release, redesigned callback-event lifetime, native hit attribution, non-MCP session launcher, Ghidra historical-action guard and actual memory-state invalidation remain prototype obligations. Existing managed Lua startup/restore qualification is not proof of the new async contract. Segmented GUI/disassembly behavior, coherent multi-read snapshots, attach/detach and advanced platform mapping remain independently unproven. Source support does not establish performance or whole-product compatibility.

Protected-mode support is not a day-one prerequisite, but the design must not make it impossible by erasing segment identity or confusing linear and physical memory. Full stack unwinding, arbitrary DOS extenders, BIOS/external-boot debugging, reverse execution, universal device inspection and concurrent controlling frontends are not implied by this proposal.

## 15. Source references

All Ghidra code references below use commit **`8ca9077040cc43cd790babcfccf19475622a3e9a`**. DOSBox paths refer to baseline **`bd053ee4e8a5b258191b3f90212244f7bd4066e7`**. The local research checkout lives outside the product repository under workspace `tmp/dosbox-y-ghidra-analysis-20261110/`; no downloaded source is added to this repository.

### Ghidra

* [Release 12.2](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.2_build) and [README/runtime requirements](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/README.md).
* [Adding a debugger](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/GhidraDocs/GhidraClass/Debugger/B5-AddingDebuggers.md): architecture, schemas, launchers, canonical/aggregate traversal and action registration.
* [Trace RMI protobuf](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-rmi-trace/src/main/proto/trace-rmi.proto): `Addr`, overlays, memory states, values, snapshots, transactions and method negotiation.
* [Python client](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-rmi-trace/src/main/py/src/ghidratrace/client.py): `VERSION`, `MethodRegistry`, snapshot/transaction/overlay/register/memory APIs, register byte order.
* [Python package requirements](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-rmi-trace/src/main/py/pyproject.toml).
* [GDB schema](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-agent-gdb/src/main/py/src/ghidragdb/schema.xml), [methods](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-agent-gdb/src/main/py/src/ghidragdb/methods.py), [commands](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-agent-gdb/src/main/py/src/ghidragdb/commands.py), [architecture mapping](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-agent-gdb/src/main/py/src/ghidragdb/arch.py): examples, not DOSBox-ready implementations.
* [TraceRmiTarget](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-rmi-trace/src/main/java/ghidra/app/plugin/core/debug/service/tracermi/TraceRmiTarget.java): GUI method matching, activation and 4KiB memory quantization; [OpenTrace](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger-rmi-trace/src/main/java/ghidra/app/plugin/core/debug/service/tracermi/OpenTrace.java): wire-address decoding.
* [x86 languages](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Processors/x86/data/languages/x86.ldefs), [real-mode processor specification](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Processors/x86/data/languages/x86-16-real.pspec), [protected-mode processor specification](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Processors/x86/data/languages/x86-16.pspec).
* [x86 SLEIGH semantics](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Processors/x86/data/languages/ia.sinc): `currentCS` (1108), `rel16` (1151), indirect near CALL (2939) and JMP (3769). These are the concrete nonaligned-CS analysis hazard in §6.3.
* [SegmentedAddressSpace](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Framework/SoftwareModeling/src/main/java/ghidra/program/model/address/SegmentedAddressSpace.java), [SegmentedAddress](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Framework/SoftwareModeling/src/main/java/ghidra/program/model/address/SegmentedAddress.java), [ProtectedAddressSpace](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Framework/SoftwareModeling/src/main/java/ghidra/program/model/address/ProtectedAddressSpace.java): encoding and alias behavior, not CPU descriptor semantics.
* [Register location tracking](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger/src/main/java/ghidra/app/plugin/core/debug/gui/action/RegisterLocationTrackingSpec.java), [Auto PC](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger/src/main/java/ghidra/app/plugin/core/debug/gui/action/PCLocationTrackingSpec.java), [frame-PC tracking](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Debug/Debugger/src/main/java/ghidra/app/plugin/core/debug/gui/action/PCByStackLocationTrackingSpec.java).
* [COM loader](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Features/Base/src/main/java/ghidra/app/util/opinion/ComLoader.java), [MZ loader](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/Ghidra/Features/Base/src/main/java/ghidra/app/util/opinion/MzLoader.java), [memory/modules/static mappings guide](https://github.com/NationalSecurityAgency/ghidra/blob/8ca9077040cc43cd790babcfccf19475622a3e9a/GhidraDocs/GhidraClass/Debugger/A6-MemoryMap.md).

### DOSBox-Y

* `src/debug/debug_common.h`: register/address/disassembly/breakpoint APIs and explicit native attachment parameters.
* `include/native_debug_execution.h`, `include/native_control_owner.h`: exclusive attachment/context/operation lifetimes.
* `include/lua_driver_handoff.h::Handoff::call`, `offerEvent`, `observerRead`, `takeVerdict` and `complete`: current blocking single-slot request/event/result handoff, not an already asynchronous execution API.
* `src/debug/lua_driver_worker.cpp::invoke`, `observation`, `module`: synchronous bindings, protected serial callbacks, read-only callback restrictions and existing result encoding. `src/debug/debug.cpp::LocalDriverObserveHit` (around 6010–6064), driver request processing (6234 onward), native enqueue (6357 onward), and result release before handoff completion (6222–6225): concrete evolution seams for async admission, control servicing and retained results.
* `src/debug/debug.cpp::LocalDriverFrontier` ExitFence (6117–6139), `DEBUG_CloseLocalLuaDriver` (6387–6434); `lua_driver_worker.cpp::sessionRead`, `sessionStopping`, `managedSession` (483–526); `include/lua_session_transport.h::wake`: current outstanding-evaluator-only delivery, blocking receive, closing predicate and destructive socket wake. These require the explicit terminal-only lifetime change, not just preserved old flushing.
* `src/debug/debug_lua.cpp::LiveStatus`, `LiveStop`, `LiveResume`, `LivePhase`, `LiveSlice`, `LiveFrame`; `src/debug/debug.cpp::Debugger_LivePhaseCompleted`, `Debugger_LiveResume`, `Debugger_LiveStop`; `src/dosbox.cpp` loop/service ordering (899–913): existing separate live Lua control and its consolidation constraints. `src/debug/debug_common.cpp::Debugger_ReadMemory` (513–538) shows the distinct side-effectful legacy read path.
* Ghidra `Debugger-rmi-trace/.../launcher/AbstractTraceRmiLaunchOffer.java::waitForModuleMapping` (646–679), `launchProgram` (682 onward): image/auto-map offer requirements relevant to R1 manual mapping.
* `include/agent/debugger_adapter.h`, `src/agent/debugger/debugger_adapter.cpp`: current register surface, selector lookup, safe memory semantics and native breakpoint restrictions.
* `include/agent/agent_bridge.h`, `src/agent/bridge/agent_bridge.cpp::EmulationThreadQueue::Pump` (100–145): emulator-thread queue, stop and target-exit listeners; stop-only overtaking during Active, complete deferral during Pending. This is a native service-reuse seam, not authorization to attach Agent alongside Lua.
* `src/agent/server/agent_server.cpp`: actual callable method list, state revisions, asynchronous execution/wait, stopped reads and current target-start contract.
* `client/python/dosbox_agent/client.py`: current Python transport restriction; `include/agent/agent_protocol.h`: native transport configuration.
* `src/cpu/cpu.cpp::GDTDescriptorTable::InspectDescriptor`, `src/cpu/paging.cpp::PAGING_InspectLinear` (lines 970–1002), `src/hardware/memory.cpp::InspectPhysicalMemory` (875–888), `MEM_A20_Enable` (1325–1340), and RAM page-handler `GetHostReadPt`: table selection, backing-memory bounds and the distinct ordinary A20 alias path. These support the limitations in §6, not runtime cache-divergence proof.
* `src/debug/debug.cpp::DEBUG_AgentCreateExecutionBreakpoint` and `CBreakpoint::CheckBreakpoint`: native location conversion and hit identity. `DebuggerAdapter::ReadMemory` (424–449) and `agent_server.cpp`'s `memory.read` branch establish whole-range success and session/revision reporting.
* [Managed Lua sessions](../managed-lua-session.md), [installed Saivage qualification](../managed-saivage-qualification.md), [fresh restore contract](../FRESH-RESTORE-CANDIDATE.md), [current foundation disposition](../REMAINING-FOUNDATION-DISPOSITION.md): existing capabilities and their scoped qualification, not evidence of Ghidra integration.

### Additional R4 consumption evidence

* `src/debug/debug.cpp::LocalDriverObserveHit` (5987–6064), boolean hit projection (6219), `src/debug/debug_common.cpp::Debugger_RecordBreakpointStop` (93–114), `include/native_stop_conditions.h::StopEvidence`: current event stack authority and missing continuous-Run registration attribution.
* Managed dependency pinned by `docs/managed-lua-session.md` at `0b8d271911c17ce762246afa86b74ab20e999343`: `lua_repl_mcp/session.lua` (39–64), `worker_protocol.lua::execution_input`, `framing.lua::feed/pop`: generic code/data evaluation, strict pre-evaluation generation advancement and newline accumulation. These are inspected dependency sources, not new product MCP semantics.
* Ghidra `TraceRmiTarget.ActionEntry` (118–130), `executeAsync`/`activateAsync` (1008–1051), `DorkedRequestCaches` (985–1004); `ActionName` enablers and EXECUTE/RESUME/INTERRUPT/STEP constants; GDB `methods.py` (583–619); `ghidratrace/client.py::Trace.set_memory_state` (365–370): concrete historical-action, console and trace-invalidation boundaries.

Prior art: [yetmorecode/ghidra-dosbox-agent](https://github.com/yetmorecode/ghidra-dosbox-agent) and [ret-sync](https://github.com/bootleg/ret-sync). Neither is selected for code adoption by this analysis.
