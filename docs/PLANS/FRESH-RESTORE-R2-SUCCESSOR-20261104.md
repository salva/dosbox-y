# Fresh disposable restore — dated r2 successor (2026-11-04)

This addendum describes the authored candidate, not independent acceptance or a
replacement of the approved historical r2 design/plan/source receipts. Governing
reviewed packet is `/work/dosbox-y/docs/PLANS/FRESH-TARGET-RESTORE-DESIGN.md`,
SHA256 `7204d48e1ab8212c7fb7cb19072786f0bd7022eda0db18122e0f50ab49be23ba`.
Production vertical: `448596413d47407283564b50684d3876fa8b7dc9`.

## Contract and use

Preconfigure matching emulated hardware and mounts, then launch:

```
dosbox-x -conf matching.conf -restore-state capture.sav -break-start
# Lua-enabled debug build:
dosbox-x -conf matching.conf -restore-state capture.sav -lua-driver-file inspect.lua
dosbox-x -conf matching.conf -restore-state capture.sav -lua-driver-code 'local d=require("dosbox"); local c=d.context(); print(d.registers(c))'
```

Startup constructs fresh native owners in original dependency order without
running BIOS/DOS/ROM/EXEC/AUTOEXEC/shell guest code. Saved owner payload installs
through the common capture/install codecs. A constructor that requires guest
callback execution (including CPX codepage decompression, such as the tested
German-layout constructor) explicitly refuses at the callback entry boundary,
before callback register/stack/cycle changes. It is not silently replayed and
that layout is not claimed supported by this candidate. Ordinary boot and guest
callbacks after restored root selection retain their existing behavior.
Import precedes debugger and Lua
FILE/INLINE evaluation. The initialized unused target is disposable: every
failed, partial or uncertain import is fatal, even if no mutation was observed.
No evaluator attaches and ordinary guest execution cannot continue on failure.
The archive remains unchanged for a new process attempt; no automatic replay.

A successful import stays stopped. Observation, host delay, repeated saves and
frontend attachment do not advance CPU/PIC/TIMER or transfer/rebudget saved CPU
cycles. Explicit step/Run alone authorizes continuation; host pacing rebases
once on activation, not installation. Repeated STATEWRITE, slot saves, selection,
info and explicit deletion remain. STATEREAD, load slot/menu/hotkey/file,
`checkpoint.load` and direct used-target native imports refuse **fresh launch
required**; force-load is not a bypass. Lua checkpoint capture remains bounded
memory capture, not permission to reload the current VM. No Lua stack is saved.
Full Lua libraries/C require/errors/finalizers/close remain normal Lua behavior.
Nondebug builds compile but no-debug native restore/root runtime remains
unqualified; they have no explicit-continuation frontend and must not auto-run.

## Safety and compatibility changes from historical r2

There is one original-derived owner traversal and codec per owner, with ZIP and
memory transports. Global Prepared/map/whole-RAM destination preservation is
removed. Progressive owning consumers validate bounds/topology/identity and
rebuild native resources; necessary local preparation is allowed. Later failure
may follow earlier installed owners and still requires disposal, never rollback
or a mixed-state success. Archive metadata mismatch is not owner validation.

Representation/engine/justified ABI, effective emulated hardware, initialized
immutable inputs and fresh owner bindings define compatibility, not whole binary
or complete host/backend configuration equality. Source/header/config/toolchain/
binary hashes remain exact evidence. Loaded ROM bytes, not reopened paths, own
immutable identity. Pointer addresses are not identity. Current broad config
hash remains overrestrictive/incomplete; this addendum does not claim §3 complete.
Real SDL video/audio to dummy backend with guest audio enabled has a positive
bounded witness, not waveform/frame/device parity.

The selected root is capture-current validated PSP/SDA/MCB ownership, not a
mutable later PSP or shell return address. Always-built typed DOS_Terminate
unwind precedes parent/INT22/file/free/stack/self-parent suffix. Root normal,
TSR and self-parent exits end the disposable invocation. Legitimate new child
exit may return to root. AH31/INT27 service prefixes need not be effect-free.
The stale-return fence is secondary. Shell-root polymorphic unwind and broader
no-debug consumers remain unqualified.

R3 retains native non-destructive reopen, shared handles/flags/cursors/errors,
current-directory searches and no host-content rollback or guest INT21 replay.
Changed directory entries alone are not failure. Current two-DTA search witness
covers matching addition/deletion/exhaustion, not all ordering/resource failures.
Publication stays per-archive atomic; cooperative deadlines include finalization
and prepublication, not preemption of serializer/fsync/C/finalizers. Close must
revoke grants, settle exact operations and response, pass Lua close fence and
only then retire attachment/worker. Blocked work retains ownership and truthful
incomplete/unknown effects; it cannot be freed beneath or silently resumed.

## Historical review and qualification mapping

Astra report on d88cc513/tree cfec814 remains historical evidence. Its original
owner advice and R1/R2/R3 findings retain their input/time scope; no later passing
probe retroactively approves that input. The reviewed packet supersedes R4
used-target preservation/global-prevalidation/exact-binary prescriptions, not
failure disposal. R5 remains cooperative, not hard native-code preemption.
Approved r2 originals and review hashes remain unchanged.

`tests/original-foundation/{README,R2,R3}.md` carry current-vs-historical notes.
`fresh-startup.py`, `fresh-root-matrix.py`, `fresh-startup-failure.py`,
`fresh-startup-no-replay.py` and Lua/terminal/close consumers replace live-load,
replay, unchanged-used-destination and all-owner-preflight assumptions. Transport
CRC/EOF/close/publication, actual CPU/memory/PIC/paging/keyboard and native-owner
safety remain covered; zip-stream is transport-only and progressive-install
checks actual common metadata/owner completion. Old pristine upstream slot
probes still describe their baseline, not candidate live-load permission.

Exact receipts, failures and outstanding scope are in
`docs/FRESH-RESTORE-CANDIDATE.md` and current card status. Remaining effective
hardware/full native extents/service/constructor coverage, exact close/frontend
races, final ingress audit, meaningful active device consumers and no-debug
qualification are not waived. Independent affected assessment is coordinated by
[[card:card-n-c-k]], wider frontend/provider/font/device/parity/composed acceptance
by [[card:card-n-c]]. MIDI is explicit unqualified lower-priority follow-up, not
universal completion gate or removed feature. This document claims no adoption,
deployment, all-hardware safety, service reset or requirement-owner rewrite.
