# Remaining foundation port: author handoff (2026-11-06)

This is an implementation increment on `b9f71b9db454d630f632f9ddfbde754c0841c689`
(tree `298adb70b011c75f7a1ec5966e6c5c2bba202754`), not whole-foundation or independent
acceptance. Historical design packets and the finite semantic map remain provenance;
the fresh-target contract supersedes their same-process/whole-preflight prescriptions.

## Delivered behavior and API

Source comparison with selected `74b5fa198` found two missing guest-owned wait
continuations. `CB_IRQ1_PAUSE` now keeps VGA Pause's interruptible wait in guest
code, with exactly one PIC EOI and return through the IRQ1 POP AX/IRET suffix.
`CB_INT15` similarly keeps real-mode BIOS AH86 waiting in a PUSHF/STI/HLT/POPF
suffix, observing the existing RTC count without restarting the wait on reentry.
Direct/APM callers retain synchronous semantics. Both use ordinary native callback
ownership; neither runs a frontend evaluator, advances time on attachment, or creates
a second restore engine. Non-VGA Pause's existing path is unchanged and not newly
qualified by the VGA workload.

The owner-engine identity is now `original-owner-engine-10`. New callback layout
and continuation semantics are incompatible with engine9 archives, even though the
outer archive representation remains revision9. Older engine9 results below prove
those earlier inputs only; they are not engine10 archive compatibility evidence.
Used-target restore remains fresh-launch-required. Initialized-unused restore stays
held until explicit Run/step; native output/provider binding and root disposal stay
owned by the common implementation. There is no replay or host-content rollback.

## Reproduction and exact build handoff

Work root: `/work/dosbox-y/.saivage/work/cards/card-n-c-l-a/`.
`build-lua/src/dosbox-x`: heavy debugger/Agent/SDL2/Lua, no OpenGL/avcodec.
Configure flags: `--enable-debug=heavy --enable-dosbox_agent --enable-sdl2
--disable-opengl --disable-avcodec --enable-debug-lua`.
`build-native/src/dosbox-x`: `--disable-debug --disable-dosbox_agent --enable-sdl2
--disable-opengl --disable-avcodec --disable-debug-lua`.
Both rebuilt with `make -j6` after engine10. Compiler Ubuntu GCC13.3.0
(13.3.0-6ubuntu2~24.04.1); NASM2.16.01. Generated config headers retained in builds.

| Input | SHA256 |
|---|---|
| Lua binary | f9d2b9d53f3bed2f69d23e4e84cc54dae3a3c66cc3d0765758c7183e19682ae3 |
| native binary | 4ad2de1403b09a6af43413dcf07245d143c22e8087c2d9a8e0f2a1b7016f97f8 |
| Lua config.h | f3b2dd74f8cb9d7526b5bd8f976af4e04d416227c1dd5bc009dfcc7663e39975 |
| native config.h | 5a6293639a5798fdb3a5160d194f3529ca42ca09e7b0be24d0c11c0b298a132b |

Set `DOSBOX_TEST_BINARY` to Lua binary. Run
`python3 tests/original-foundation/fresh-root-matrix.py`;
optional `DOSBOX_TEST_CASES=ah86-held,pause-held,keyboard-d1-held,gameport-timed,gameport-untimed`
selects exact cases and rejects unknown names. ASM is authored in this directory,
assembled deterministically with NASM. Set `DOSBOX_TEST_NATIVE_BINARY` to native
binary for `fresh-native-console.py`. Run `fresh-e2e-milestone.py` for debugger,
FILE and INLINE consumers. Native no-Agent binary cannot serve the Bridge socket;
use its actual `-restore-console`, not the socket harness.

## Author evidence, with applicability

Engine10: `work:///cards/card-n-c-l-a/processes/proc-e806c340ce45/stdout.log`
(exit0) records all39 fresh-root cases, native console, and four combined
save/fresh-process FILE/INLINE restore/held observation/explicit continuation/root42
workloads. Raw receipts under `fresh-root-matrix/run-kkwppssv`,
`native-console-l1_nmjrp`, `e2e-milestone-elf2tivu` in the work root.
New held cases include active AH86 and Pause interrupt frames, pending8042-D1 with
unread synchronous D0 output, partially consumed untimed gameport counts and timed
discharge. No physical joystick/keyboard input claim. Existing cases exercise
shared handles/current file content and searches, XMS/EMS/HMA/UMB/A20/hooks,
mouse/video/VESA/keyboard services and active SB FIFO/speaker/timer state.

Engine9 dirty-source frontend regression chain exit0:
`work:///cards/card-n-c-l-a/processes/proc-8d29b6c63520/stdout.log`:
exclusive runtime (persistent Interactive expressions/sink/error repair/ownership),
RPC (including REAL_RPC_STOP_CONDITIONS_PASS), production Lua (full libraries,
C require, file/callback/finalizer), four E2E cases. Chain used `&&`, not an exit0
from an unrelated final command. Those nonarchive frontend implementations are
unchanged by engine10. It is not installed-host/native-MCP certification.

Reduced actual-code UBSan probes: keyboard, timer, XMS, EMS, mixer, render passed
in proc-f8fe0b34b92c before DOS fixture compilation failed (stale missing native
binding/HMA stubs). Repaired reduced DOS fixture passes proc-898e19adda36;
DOS allocator all65536 strategy values also pass there. DOS-table fixture's old
expectation of replacing a different native allocator cursor failed there; actual
owner deliberately requires equality to avoid overwriting retained allocations.
Updated assertion now tests that refusal. Pass recorded proc-34747fe9407f, followed
by stale MSCDEX stub compile failure (missing callback binding / bool audio results).
Updated MSCDEX stubs preserve focused inventory/audio tests; proc-7395887e3e2e
passes MSCDEX, native MSCDEX callback identity and HMA probes. Its subsequent
diff check failed on two CRLF new lines; normalized only added lines, final
`git diff --check` passes proc-5559b9d1c1ef. Reduced probes do not qualify media playback.

Earlier incorrect keyboard D2/A5 fixture root99 and no-Agent socket listener timeout
are not hidden: corrected synchronous D0 fixture and actual native-console route
pass. These were fixture/route failures, not evidence of a fixed device defect.

## Remaining scope, not acceptance by mapping

Finite map/ledger is a work aid, not line-by-line runtime certification. IDE's
selected net delta is only IO02 catalogue registration; its removal does not
implement nonempty IDE state. Need source-grounded disposition and useful common
media/geometry/provider/font variations, remaining real consumer coverage and dated
project §7/API/CLI claims before whole delegated completion. MIDI remains explicitly
unqualified lower priority; exhaustive hardware/composed testing stays with parent,
installed host/MCP/Saivage with its owner. No claim of nondebug integrated autosave:
unsafe timed callback refusal plus held save is the qualified behavior. External
boot/nonempty device/shell root/unrepresented lazy/LFN limits remain explicit.

No service, branch switch, merge, rebase, publication or cleanup was performed.
Inherited unknown processes/resources and caches remain preserved. New tests own
their run directories; full raw captures are not committed. Independent affected
assessment remains required and is arranged by the parent.
