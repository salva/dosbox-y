# MPU parser / SB native binding continuation (2026-11-06)

Engine29/representation9. The original MPU owner formerly saved `mpu` but
`MPU401_WriteData` held length/count/position in function statics. Selected
74b5fa198 moves those three into its owner. The same direct owner change is now
made here: ordinary parsing and the original POD traversal share one history.
No parallel MIDI codec, byte replay, callback, clock or port operation at import.
MIDI host delivery remains explicitly unqualified; this repair does not certify
physical synthesizers or queued host output.

`tests/original-foundation/mpu-parser-history.py` compiles the actual owner,
WriteData and original save/load with an explicit byte sink. A channel note-on
is saved after status, then resumed in a zeroed fresh owner; system song-position
is saved after status/first data and resumed with its final byte. No import
output occurs; future bytes and completion phases match. ASan/UBSan pass.
Other parser branches and malformed MPU structs are not certified by this test.

SB's existing both-card owner codec already resolves saved DMA role and retains
destination MixerChannel. Its raw logical snapshot nevertheless emitted those
two source addresses. Capture now clears both native fields in a local copy;
original live bindings are untouched. The separate DMA role is unchanged, and
load still reconstructs destination bindings without callbacks. This is not a
new portable ABI: remaining scalar/array POD representation is same-ABI and
padding is not declared canonical. The binding regression now checks the wire
contains null native bindings as well as both-card combinations, all DMA IDs,
unavailable/unknown DMA, every truncated prefix and consumed DSP bounds.

Evidence:
* proc-4a0b24982b66 exit0: both actual-code sanitizers, all three configured
  builds, diff check. Build logs in card work `mpu-sb-history-{lua,native,debug-no-lua}.log`.
* proc-c28e2d0df46f exit0: real exclusive Interactive/Bridge, RPC stop conditions,
  production Lua callback/file/code/save and four combined debugger→fresh
  FILE/INLINE held→explicit continuation→root42 disposal E2Es, receipts
  `e2e-milestone-8qzyxt_r`, `rpc-runtime/run-lmcbpkmp`,
  `lua-production-runtime/run-yoqzfxht`.
* Earlier proc-43632d4cefec exit0: all47 engine28 fresh-root cases, receipts
  `fresh-root-matrix/run-3uztjuz_`; not engine29 archive interchangeability.
* Engine29 pre-SB-redaction proc-00645780055b five no-Lua fresh consumers and
  four combined E2Es passed; later SB capture changed binary hashes, so that
  earlier receipt is not the final SB evidence.

Final binary SHA256 before the subsequent GUS-address-only repair (all profiles SDL2/FreeType/printer/TTF, GCC13.3/NASM2.16.01;
configured headers/status unchanged from TANDY-NATIVE-CONTINUATION.md):
* Lua heavy-debug/Agent: dca4f314763ab700d5692cc530de62175102ede820ef5b9fb81093329b594199
* native no-debug/no-Agent/no-Lua: 387ae4bd0f73b579df2df4618d6faab572cb45e1ec46566d33d829a5da9287c7
* heavy-debug/Agent/no-Lua: 57b60e9e69934d678efd868ad4d9204a0ba12844aa57f2f297e57e02aeeeaac4

Earlier engine archives are incompatible; no force-load used. No runtime API,
CLI option, save policy, publication or used-target admission changed. This is
a bounded owner correction, not whole-foundation independent acceptance.
