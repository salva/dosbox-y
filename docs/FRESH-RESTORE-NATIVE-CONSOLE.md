# Native no-debug console vertical — author handoff

Cause established from actual initialization: sdlmain calls CALLBACK_Init then
C_DEBUG-only DEBUG_Init before VGABIOS and guest owners; DEBUG_Init allocated
lowest free slot, shifting every subsequent owner by one. CALLBACK_Allocate
scans upward. Shell bindings correctly compare slots/handlers/types/location/
spans/bodies; bypassing them would be unsafe. Both DEBUG_Init and BIOS-reset
DEBUG_ReinitCallback now use shared allocator's high-end frontend allocation.
No dummy debugger callback in no-debug, remap, replay, second engine or weakened
owner comparison. Capacity still rejects safely; guest allocation order is
independent of optional debugger until genuine slot exhaustion. Old archives
with old slots can reject unchanged strict binding checks; no migration promised.

No-debug `-restore-state archive -restore-console` is a small exclusive native
startup control: blocks on stdin before any guest frames. `status` prints held
cycles/left/PIC/ticks, `run` alone activates pacing and enters original RunMachine,
`quit`/EOF/invalid input dispose with failure. Without console still refuses
continuation. Console without archive or on debugger build rejected. No Agent,
Lua, worker, polling thread or alternate execution engine; no implicit guest
advance on host delay/status. Standard input must be non-secret/trusted command
input. This is not new RPC/interactive debugger parity.

Test fresh-native-console.py captures real active SB FIFO through heavy-debug
producer; distinct no-debug/no-Agent/no-Lua binary restores stopped, waits2s,
status timing invariant, explicit Run consumes reset0xaa/E1 and root42 disposes.
Valid-CRC Shell binding mismatch refuses before console; quit refuses without
root suffix; archive unchanged. Full four-workload Lua debugger/FILE/INLINE E2E
passes proc-77b465231ed5 on owning production delta (before help-only line).
Final help-inclusive rebuilt binaries + native vertical/mismatch/quit pass
proc-23817bb19745: native-console-eqzg97_5 under card-n-c-k-a work root.
Lua SHA256a18d836a7ab864ed57e6a8911c8f98e7dcab71c0d090c02d22ce023c8c8d8e1a;
no-debug265f6674894d32415b6c9b112b86b0457f4b6e741cad9a8500e44e3d4e4f2857.
Config/header/status hashes proc-77b465231ed5; unchanged on final rebuild.
Build logs build/frontend-{order,final}-*.log, frontend-order-no-debug-fix.log.

Initial proc-2240dde9007c failed diagnostic invariance: pic_tickindex_t is not
double, printf needed explicit cast; repaired owning diagnostic. proc-e7e566b938e7
failed test mutated nonexistent Shell instead of Shell bindings, actually ran
valid root42; corrected member. proc-b3afbd738a13 correctly rejected binding but
asserted verbose debug-only disposal text; now checks actual native failed-owner
exit1/no attachment/no termination. Sequential later E2E skipped in those failed
commands, not passes. Missing serialize paths were read diagnostics only.

Parent affected independent handoff: inspect original allocation order/high-end
allocator and strict owner rejection, exact no-debug config, held console/status
and explicit SB/root42, rejection/quit and debugger E2E preservation. Author
checks are not independent acceptance. k-b status4 approves prior953 fixture
correction and status3 production2c0c, not this candidate. Wider effective
hardware/native extents/ingress/close/docs remain actionable; no all-device gate.
