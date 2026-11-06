# GUS native DRAM address omission (2026-11-06)

Engine29/representation9, successor to d6f509a9e. `POD_Save_GUS` now writes a
local GFGus descriptor with null GUSRam, followed by the existing exact DRAM
bytes. Source native allocation is unchanged. Load already checks installed
geometry and retains destination allocation; no new loader/validator or replay.
The change preserves payload size and all logical fields; earlier engine28
archives remain incompatible with29 because of the MPU owner change.

`gus-dram.py` compiles the actual GFGus fields and save/load consumption. It now
asserts null wire address and untouched native allocation in addition to nonempty
content, truncation and geometry mismatch. This does not certify full GUS/MAX
synthesis or physical audio. Selected74b5fa198's GUS net change is installed-device
absence observation only; upstream UART0.32ms/PIC and actual-port SB forwarding
remain retained, not replaced by the old stub.

proc-7cf868934f0a exit0: ASan/UBSan DRAM check, all three builds and diff check;
logs `gus-address-{lua,native,debug-no-lua}.log` in card work.
proc-655b2410e3f2 exit0: actual GUS Tx/PIC/SB port regression, all relocation
indices/failed-stream preservation, PIT held-clock consumption, four combined
FILE/INLINE fresh E2Es (`e2e-milestone-3bu2egw6`) and actual native no-debug
SB FIFO held/status/delay→Run/root42/mismatch/quit disposal
(`native-console-cnnw1lz9`). Author evidence, not independent acceptance.

Current binary SHA256:
* Lua: f1487e31bb326c4cbe901745fe15d8482db451f9e526cf970a6ab1be908c10d7
* native: 203956d154bc4d2f3af2c30be6e3758147af91f246c5c3b51a52cad88d552cbd
* debug-no-Lua: d0d0b18a1b0b52d6b0213ebc0dcfe46c9e0fc831c8012e370338e3f565d42d75
Configuration/header/toolchain identities remain those in
TANDY-NATIVE-CONTINUATION.md. MPU-SB-OWNER-CONTINUATION.md's earlier binary hashes
are explicitly pre-GUS, not final candidate identities.
