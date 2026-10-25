# Original foundation author probes

These candidate probes are **not** foundation acceptance. The pristine baseline
is selected upstream130de209abd6e6fb55b5535f85e2af4f6086ed58; candidate production
now begins shared VGA arithmetic and original PIC/GUS callback seams, not the
complete debugger/save port.

## Completed-render identity

`python3 tests/original-foundation/completed-frame.py` checks selected native
clock and actual registration seam under ASan/UBSan: eligibility combinations,
unchanged-frame identities, observer exclusivity/detach and exhaustion without
wrap. `RENDER_EndUpdate` synchronously supplies a borrowed cache/palette view to
a bounded native observer; callbacks may not pause, mutate renderer state or
retain pointers. This is not completed-PNG delivery or consumer/resource lifetime
qualification. No selected renderer save codec/dispatch catalogue is imported.

## Renderer allocation

`python3 tests/original-foundation/render-allocation.py` compiles the actual
allocation bodies with a poisoned allocator and ASan/UBSan. It checks complete
source-cache initialization including slack/guards, rejection/allocation failure,
size overflow refusal and every odd-height changed-line guard entry. These are
selected non-save confidentiality/initialization fixes, not presentation rebuild,
completed-frame observation or real output/resource lifetime qualification.

## Logical gameport

`python3 tests/original-foundation/logical-gameport.py` compiles actual original
port handlers and the selected logical initialization under ASan/UBSan. Checks
cover centered initialization, 64-read discharge, timeout, timed deadline and
existing second-stick swapped-axis/button behavior. `joysticktype=logical` retains
the selected non-save centered gameport without SDL discovery/bindings. The
original single serializer continues to own deadlines/counters/write state; no
JL/JU/JN codec or registry is imported. This extracted probe is not SDL lifetime,
full input/save admission or fresh-process continuation qualification.

## POSIX RPC transport

`python3 tests/original-foundation/unix-transport.py` compiles the actual transport
with C++14/Werror/ASan/UBSan and exercises fragmented and multiple newline/CRLF
requests, the exact message bound, an oversized connection, handler exception,
idle-client Stop, restart, existing-path refusal and replacement-path retention.
The selected non-save Unix transport is ported onto the original agent transport
owner. It requires an owner-private directory, publishes a mode0600 socket and
never removes a pre-existing endpoint. Disposal only removes the recorded inode.
It serially serves connections as in the selected implementation; this does not
qualify guest/native execution ownership, stop overtaking or full RPC lifetime.
Handler failure closes its connection without replaying possibly-effectful work.
Listener and accepted descriptors are close-on-exec; Linux uses atomic socket /
accept4 flags so concurrent Lua worker exec cannot inherit them. Other POSIX
platforms use fcntl and retain an unqualified concurrent-fork window. The Linux
fixture checks both actual production socket FD flags (ignoring unrelated
inherited sockets) and counts the throwing handler once, without replay.

## Agent build feature selection

Agent translation units and SDL startup must include generated `config.h` before
checking `C_DOSBOX_AGENT`/`C_DEBUG`. Automake supplies `HAVE_CONFIG_H`, not those
feature macros directly. Otherwise agent source files silently compile empty and
SDL startup sees agent declarations only after their guarded includes have been
skipped. The first actual enabled-agent build exposed the SDL declaration error;
the source-level repair restores normal configured compilation, not a forced
compiler define. An enabled linked build and runtime self-tests are required;
an agent-disabled archive or manually forced syntax check is not equivalent.

## RPC memory observation

The RPC read primitive now consumes `MEM_InspectPhysicalMemory` or
`PAGING_InspectLinear`, rather than ordinary physical handlers or checked guest
reads that may dispatch MMIO or modify paging state. `rpc-read-observation.py`
compiles that actual routing body with observer doubles and checks all address
spaces, explicit refusal reasons, bounds and unchanged refused byte. Existing
`memory-observation.py` checks the real observer bodies and
`rpc-read-publication.py` checks whole-result publication. Unsupported device or
alias memory is explicitly not inspectable; no effectful fallback is used.
These are not live RPC admission, write, execution or session-lifetime proofs.

## Descriptor observation

`python3 tests/original-foundation/descriptor-observation.py` checks the actual
GDT/LDT descriptor inspection body: complete inclusive descriptor limits, overflow,
explicit little-endian decode and unchanged output on refusal. It consumes the
shared non-mutating linear observation primitive rather than `Descriptor::Load`
(which changes mpl and uses ordinary guest memory reads). The fixture substitutes
paging; `memory-observation.py` separately covers that primitive. The production
RPC segmented resolver now consumes this API rather than the effectful lookup.
`rpc-segment-observation.py` compiles both actual bodies together with CPU/paging
doubles and checks real/v86 wrapping, protected-mode normal/expand-down limits,
GDT/LDT routing, refusal and overflow. Its first run failed on an incorrect
real-mode expected address (0x123c5 instead of wrapped 0x23c5); the corrected
expectation preserves the existing 16-bit wrapping contract. Live RPC
context/attachment admission and memory read/write migration remain outstanding.
`rpc-read-publication.py` checks publish-last in the actual RPC ReadMemory body:
translation/read refusal at every position leaves the caller's prior result
unchanged, while a full read replaces it. This is output publication correctness,
not proof of side-effect-free guest reads; ordinary read handlers, write probing
and native session authority remain to be migrated/qualified.

## Shared native dispatch restoration

`python3 tests/original-foundation/dispatch.py` compiles the actual shared
`DOSBOX_DispatchDecoder`/`DOSBOX_DispatchCallback` function bodies with deterministic
native doubles. Both normal-loop and debugger callers now use these same
primitives. The fixture checks decoder and callback normal return, exceptions,
callback identity/flag restoration, and invalid callback indices before effect.
This preserves incoming page-fault policy and callback identity across unwind;
it does not itself establish frontier/save admission, core/flags borrowing, or
session disposal. Real caller/build/runtime checks remain separate.

The machine-service coordinator is now linked at `DOSBOX_RunMachine`: requests
remain data-only until a zero handler return, with no service on native return
or exception. Root, natural shell EXEC, callback/idle, fault and disk invocations
have explicit kinds; unclassified calls still deny service. Shell EXEC uses a
named wrapper rather than inferring authority from INT21 or register values.
`dispatch.py` also exercises the actual coordinator wrappers, zero/nonzero seam,
request retention under a callback ancestor and expiry on invocation return.
This does not yet register a frontend evaluator, bind native frontiers, settle
execution on owner departure, or admit any save operation. Classification alone
is neither execution nor save authority.

Native frontier scopes now surround actual decoder/callback entry and callback
wrapper setup/restoration. Normal loop predispatch/zero decoder/completed tick,
nested entry and zero-handler return expose borrowed descriptors; nonzero
callback tokens still reach their restoration caller first. Ordinary callbacks
retain a restricting suffix through NativeReturn; natural shell EXEC uses its
named ancestry rather than pretending to be an ordinary callback. No emulator
frontend binds this hook yet, and these seams do not themselves admit saves.
`callback-frontier.py` extracts the actual INT/ShellExec wrappers to check normal
setup/restoration, restricting descriptors and exception scope expiry. Upstream
register restoration on exceptional exit is not invented or claimed. The
`dispatch.py` fixture checks actual entered scopes under normal and throw paths.
The optional CPU slice observer receives before/after notifications at the shared
decoder primitive (normal and DEBUG_Run callers); after observes restored policy
also during exception unwind. Observer callbacks cannot throw, execute guest or
pump services. Its binding does not grant input, execution or save authority.

## Debugger message sink

`python3 tests/original-foundation/message-format.py` runs the actual formatting
and newline-trimming fragment under ASan/UBSan, including oversized messages.
The sink uses the stored length rather than vsnprintf's would-have-written count;
formatting errors leave an empty message. This preserves selected non-save sink
behavior but does not qualify ncurses, capture delivery or session lifetimes.

`diagnostic-capture.py` compiles the actual RPC diagnostic tail and GUI capture
functions under ASan/UBSan. Parser exceptions restore the capture/paging owner
before propagating, without replay or replacement output. Normal rejection still
returns captured diagnostics; nested begin refuses without stealing the active
capture. End restores paging once, transfers its string without allocation and
releases retained contents; an inactive end cannot restore stale paging values.
This is synchronous sink ownership, not full native session or ncurses lifetime
qualification.

`bridge-capture.py` executes the actual native MCP Bridge command body with
command/sink doubles under ASan/UBSan. Ordinary EXEC and BREAK exceptions end
capture before propagating the original failure, without replay or a partial
response; rejection and subsequent requests retain normal capture behavior.
This does not qualify Bridge connection/attachment retirement, execution admission
or transport delivery, which still need the full native facade port.

## Breakpoint identity dependency

`python3 tests/original-foundation/breakpoint-identity.py` runs actual native
constructor, delete-by-identity and retained-hit bodies under ASan/UBSan. Adapter
handles are monotonic nonzero identities, never allocator addresses; stale handles
cannot delete a replacement, and explicit deletion clears a retained hit. One-shot
hit identity can survive native object retirement without dereferencing freed
storage. The fixture also executes actual interrupt AH/AL matching and one-shot
retirement, address/index/all explicit-deletion routes and receipt invalidation.
All builds now share pointer deletion (including no-agent builds), so address
removal deactivates patched instructions and clears retained identity before free.
This is an owner seam, not full attachment or RPC transport qualification.

`python3 tests/original-foundation/breakpoint-patches.py` additionally compiles
the actual non-heavy breakpoint class and activation, physical-hit, lookup and
deletion bodies. A bounded byte array stands in for instruction memory; actual
patch writes are observed rather than replacing Activate with a no-op. All four
explicit removal routes restore the instruction, co-located permanent/temporary
patches survive either deletion order, bulk retirement unlinks freed peers before
subsequent activation searches, one-shot hit receipts survive retirement,
and a guest INT3 is not replaced with invented bytes. This closes the extracted
non-heavy patch-restoration coverage gap, not a real non-heavy emulator build,
instruction-execution, physical bus, attachment or transport lifetime check.

## Held debugger loop dependency

`python3 tests/original-foundation/held-loop.py` compiles the actual DEBUG_Loop
body with host/UI spies. Repeated held turns preserve guest registers/ticks and
never deliver PIC IRQs or synthesize an implicit resume; host polling, keyboard
handling, redraw and the explicit running-loop branch remain. Pending IRQs wait
for the next explicit execution grant. This ports selected held-loop semantics,
not full execution-owner/frontier integration or live frontend qualification.

## Text observation dependency

`python3 tests/original-foundation/text-observation.py` exercises the actual
`VGA_CopyTextSnapshot` body with native-owner doubles under ASan/UBSan. It checks
planar character/attribute stride, wrap, full-region bounds and refusal preserving
prior output, with no backing mutation. Production ports the selected non-save
observer over the current upstream drawer; only the selected ordinary VGA mode3
80x25 layout is available. This does not qualify live rendering, stopped/context
admission, text-stop consumer integration or resource lifetimes.

## Native primitives and worker (not yet emulator-integrated)

`python3 tests/original-foundation/memory-observation.py` compiles the actual
physical/linear observation bodies with bounded owner doubles under ASan/UBSan.
The shared APIs reject unsupported spans without exposing partial output and
avoid faults, TLB fills, page-bit writes and device reads. Physical observation
is installed low-16MiB RAM/ROM backing only, not an arbitrary bus read or A20
translation. These APIs do not grant stopped/context authority and are not yet
consumed by the production debugger; live paging/device qualification remains.

`python3 tests/original-foundation/flags-observation.py` compiles the actual CPU
lazy-flags owner against native headers. It compares observational flags with
normal materialization across lazy operation types and representative operands,
including repeat reads and preservation of deferred state. The existing RPC
register adapter now consumes this owner operation instead of stale `reg_flags`.
This does not establish register-context admission or integrated RPC lifetime
correctness; those remain part of the frontend migration.

Run `python3 tests/original-foundation/native-primitives.py` with g++, pthread,
pkg-config and Lua 5.4. It compiles actual selected non-save owners and worker
with `-Wall -Wextra -Werror`; outputs live only under
`$SAIVAGE_CARD_WORK_ROOT/native-primitives`. Headers/worker and existing fixtures
are preserved from74b5fa198e619686d506d152b0edcd1d52d5bd0f, without codec families.
New `tests/machine-service/execution.cpp` covers exclusive attachment, stale
context, Pending/Active/Terminal, retained result, attachment/owner departure,
unlimited RUN and finite step budgets. Selected fixtures cover call-local
frontier authority, classified service, stop arbitration, worker file/code,
full libraries/require/observations/finalizers and cooperative close.
These passing primitive/transport checks do not establish machine admission:
CLI selection, actual frontend integration, semantic save joins and real native
resource/lifetime qualification are still unfinished. Worker is not yet linked
into the emulator. No complete usable debugger handoff is claimed.

## Ordinary upstream baseline

Use a separate upstream build and a new disposable directory on each invocation:

```sh
python3 tests/original-foundation/upstream-slot.py \
  "$SAIVAGE_CARD_WORK_ROOT/upstream-baseline/src/dosbox-x" \
  "$SAIVAGE_CARD_WORK_ROOT/ordinary-slot-example" --memory 32 --cycles 6000
```

Requires nasm, Xvfb, libX11 and Python3. The helper allocates its own X server;
it never uses inherited display106. It sends events only to a uniquely matched
DOSBox window, runs the fixture, checks live counter/tick/input records, loads an
ordinary slot, closes normally, then starts another emulator and loads a retained
first-process slot with autosave disabled. The helper waits on its owned processes
and retains logs/receipt/config/fixture/archive in the supplied directory. It uses
SDL dummy audio: PIT register variation is **not audible output qualification**.
A missing load acknowledgement or guest-input observation is failure. Slot ZIP
CRC is reported, not a semantic state-equality assertion. Slot snapshots in the
receipt precede the load and may subsequently be overwritten by autosave; the
retained.sav is copied separately for the second run. Config changes between runs
are recorded by per-run config hashes.

Observed upstream baseline with binary SHA256
`327cc1d299bf2fa0e3ed7f51599d1c6dde1bf14401fe6f0cd22ab3c61844308d`:

- 32MiB/6000 same/fresh process load acknowledgement and normal exit:
  `work:///cards/card-n-c-a/upstream-slot-fresh-load/receipt.json`.
- 48MiB/9000 same/fresh process load acknowledgement and normal exit:
  `work:///cards/card-n-c-a/upstream-slot-48mb-9000/receipt.json`.
- 32MiB/6000 plus actual injected key record:
  `work:///cards/card-n-c-a/upstream-slot-input/receipt.json`.
  Runs report respectively24576/5461 distinct counter values and156/36 distinct
  BIOS tick values, key observed, load acknowledged and exit0.

These are deliberately bounded witnesses, not restored-register/RAM equality,
no-progress stopped inspection, XMS/EMS allocation, broad DOS-program or native
resource/lifetime coverage. Fresh process starts the fixture before loading; it
is not a fresh STOPPED debugger bootstrap. The fixture creates LIVE.BIN at startup:
its earlier file content is **not** a restoration oracle. Successful load logs do
not establish that all internal state or file cursors were restored correctly.
FluidSynth missing SoundFont diagnostics remain; MIDI is unqualified. Initial
F12+q close attempts timed out; WM_DELETE_WINDOW is the tested close approach.

## Corrected pristine baseline

Detached source130de209/tree6639975e876405ede1eee60dfb79fe5c7d23d63a was verified against all7829 tracked Git blobs with zero mismatches (`work:///cards/card-n-c-a/pristine-source-verification.json`). Additional generated autotools files are not claimed tracked upstream. Fresh `pristine-final-build` configure completed before make, using separate bare metadata pinned130de209 (configure reports130de20), flags `--enable-debug=heavy --enable-sdl2 --disable-opengl --disable-avcodec`. Build `proc-60ecbc06b09e` exit0; warnings remain. SHA256 identities:

- binary `aa145d4df9bd04e44f0fcd7413d13c2be1fe222bf393e4766bf7d981e18fd495`
- config.h `b737cfd5f99b83b337403aae3f1609e655abb3fcdb310d248860e60ea1eb016e`
- config.status `c2c04867b6e08ec80032a2d86a88268071d32cc525b332ec12927277c9cd9204`
- fixture `ae107bfe1dc63f7d0fc2bbbc7933100e5de574cd4fd50fabc52fa2cbc12a25ec`

Use `pristine-final-build/src/dosbox-x` instead of the older example binary above. Corrected runtime receipts supersede older build evidence without erasing its metadata/overlap limitations:

- `work:///cards/card-n-c-a/pristine-final-slots-32/receipt.json`,32MiB/6000, same/fresh counters25258/5461 and BIOS ticks161/36 (`proc-3bbbd36ad405` exit0).
- `work:///cards/card-n-c-a/pristine-final-slots-48/receipt.json`,48MiB/9000, counters36864/8192 and ticks157/36 (`proc-4c1949ec591e` exit0).

Each records exact per-run config hashes, two normal exits, input/load acknowledgements and five CRC-valid ZIP observations. All semantic and resource qualifications above still apply; this is not fresh STOPPED debugger restoration.

## VGA 3270 stride regression

```sh
python3 tests/original-foundation/scan-stride.py
```

Compiles and executes actual `VGA_CheckScanLength` and its shared
`vga_scan_addressing.h` primitive from the candidate with small model globals.
Compares every enumerated mode/model/card and address shift0–2 for both architecture
branches with the pinned pristine-upstream function, then explicitly checks CGA2 and CGA4 on MCH_3270PC with two row totals and
ordinary CGA/EGA/MCGA/integrated cases. The shared resolver retains the upstream product; future reconstruction callers
must pass actual address_line_total. This is an arithmetic
regression, not a rendered-screen or whole owner reconstruction test.

## Original POD owner publication

`python3 tests/original-foundation/pod-owner.py` compiles the actual
`SerializeGlobalPOD` body with ASan/UBSan. Every short prefix, prefailed stream,
and exception-enabled short stream preserves both live fields; a complete
payload installs both. The owning primitive now buffers its complete registered
payload before assignment. The original sequential slot loader consumes stream
failure as fatal installation failure, never a successful load or resumable
pre-effect refusal (also exercised with a fatal stand-in). Missing later
components and ZIP close/CRC failure after installation starts are also fatal;
pre-install refusal does not invoke `flagged_restore`, while successful install
does. These completion branches are exercised with a fatal stand-in. This retains native byte layout and registration
order; it is not semantic validation, native-resource preparation, complete
archive validation or multi-owner atomicity. `readString` likewise publishes
only a complete consumed string and grows by consumed chunks rather than
allocating an unchecked declared length immediately. The same probe checks
all truncations of empty/small/8193-byte strings, exception-enabled reads and
a maximum-size declaration with no payload. This is not an archive size limit
or semantic string validation. Original slot loading still has
unsafe sequential install/metadata/error behavior and is not qualified as the
unified save join.

The completed-frame probe also extracts actual EndUpdate view construction and
checks source cache/palette pointers, dimensions, pitch, flags, tick and sequence,
plus aborted/inactive refusal. It still uses fixture renderer globals, not a
live output consumer or native framebuffer lifetime qualification.

## GUS/PIC and SB preservation seam

```
python3 tests/original-foundation/gus-pic.py
```

Compiles the actual GUS Tx write case/callback, PIC event/timer tables/index
accessors, original SerializePic getBytes/setBytes, and SB forward/read primitives
with inert other device targets and a small queue/global fixture. Exercises0.32ms
and value admission, callback-index roundtrip, loopback/output branches,
actual-port forwarding, serialized event value/time, queue relocation and ticker
list reconstruction/disposal. ASan/UBSan are enabled. Invalid event/timer indices
fail through the actual fatal boundary rather than out-of-bounds table access.
The production loader also rejects truncated/invalid queue links and heads;
these partial-install failures are fatal, not pre-effect refusals or rollback.
Original PIC table appends GUS_MidiTxDone without shifting prior indices.
This is not live full-machine archive reconstruction, real MIDI/audio or native
resource/lifetime qualification. Initial generator failures (NULL/duplicate
stand-ins, unsigned fixture constant unlike production's integer macro) were
corrected without production changes to accommodate them.

## Native input vocabulary and observational profile

`python3 tests/original-foundation/input-primitives.py` compiles the selected
`include/native_input.h` vocabulary/cursor primitive under ASan/UBSan. It checks
all admitted names, null/unknown names, endpoints/rounding, first-delta and
invalidation, coordinate/dimension/overflow refusal. Keyboard tuple/profile
queries retain selected semantics without importing save codec/whitelist logic.
These queries are observational; they grant no input, state or execution
permission. Frontend ownership, release/close, viewport lifetime and save-input
joins remain to be connected and qualified. Raw input is not yet lease-gated.

## Consumed CPU decoder identity

The actual lazy-flags loader now buffers its complete native record and refuses
unknown operation IDs or non-boolean saved carry before assignment. CPU installation
checks failure before paging/decoder transitions; earlier CPU changes make this
installation-fatal, not pre-effect refusal. `flags-observation.py` compiles the
actual owner under ASan/UBSan: every truncated length preserves old flags,
malformed operation/carry is refused, and valid operations roundtrip before
observation-vs-materialization checks. The archive layout is unchanged; this is
not complete CPU semantic or saved continuation validation.

`python3 tests/original-foundation/cpu-decoder.py` exercises the actual original
CPU decoder map and consumed identity guard under ASan/UBSan across eight
compile-time core combinations. All 65536 identities are checked per combination;
short-stream status is fatal even for a known identity. Unknown IDs no longer
silently choose a trap/HLT range or preserve an arbitrary core. This is an
installation-time fatal boundary after CPU fields may already have changed,
not pre-effect owner preparation, CPU semantic validation or fresh-process core
reconstruction. The existing valid-ID transition logic remains unchanged.
The reverse identity helper now maps its actual argument, not global
`cpudecoder`: HLT-to-trap restoration queries `cpu.hlt.old_decoder` and must not
mistake the active HLT decoder for that saved continuation. The same eight-way
probe verifies pointer/ID roundtrips for every available core and unknown-pointer
refusal with an unrelated global decoder absent from the fixture.

## Paging host-cache reconstruction

`python3 tests/original-foundation/paging-rebuild.py` extracts the real loader
and `PAGING_InitTLB`, checks hostile serialized link indices/count, every short
payload, fault-queue overflow and all host pointer/handler/count resets under
ASan/UBSan. The original loader forced `links.used=PAGING_LINKS` then walked
archived indices, including unused tail entries, before clearing the full cache.
It now rebuilds via the existing full reset without consuming archived indices.
Incomplete reads and impossible fault-queue counts are installation-fatal, not
pre-effect refusal. The existing archive layout is unchanged. Serialized pointer
bytes remain in that legacy layout but grant no host-cache authority. This does
not validate guest paging semantics, reconstruct a suspended native fault stack,
qualify large-profile performance or supply unified save admission.

## ZIP stream ownership

`python3 tests/original-foundation/zip-stream.py` builds the bundled minizip C
sources plus actual C++ streambuf header under ASan/UBSan, and checks a real
100003-byte/two-member roundtrip, EOF and repeated explicit close followed by
destruction. Streambuf close now consumes the member once and retains its error;
a destructor cannot close a subsequent member or overwrite close uncertainty.
Reads/writes are chunked to minizip's signed-result range, retaining partial
counts and errors rather than narrowing `streamsize`. This does not establish
atomic publication, fsync durability, complete archive preflight or resource
preparation. Initial system `-lminizip` link failed because unavailable; the probe
uses the actual bundled implementation instead. Formatted metadata insertion
and `put` also exercise the actual single-character overflow path; without this
streambuf override, numeric metadata/newlines can set badbit and silently stop
writing even though ZIP member close succeeds. The probe also corrupts an actual
stored member's payload and verifies CRC failure remains observable on both first
and repeated close; it does not claim the archive is valid because a stream read
returned all bytes. The real save-owner completion seam also receives a failing
serializer against a valid ZIP member: stream failure must defeat successful
member close, and failed saves must not invoke the post-save backup notification.
This is truthful completion reporting, not preservation of an overwritten slot.
