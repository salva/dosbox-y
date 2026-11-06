# VGA cursor native-color port (2026-11-06)

Source comparison selected74b5fa198 versus029bd65, `vga_draw.cpp`, exposes one
remaining ordinary-path delta distinct from checkpoint codec factoring: byte-array
cursor colors were read through potentially unaligned/aliasing uint16_t/uint32_t
pointers. Port selected `vga_cursor_color.h` unchanged and use it at all16 typed
load sites in the existing 24-conversion/VGA/LIN16/LIN32 cursor producers.
Native-endian interpretation, foreground/background order, XOR branches and
byte-only cursor paths remain unchanged. No extra codec/validation/guest drawing
or time advance. Engine21/representation9 unchanged.

`tests/original-foundation/vga-cursor-color.py` audits all16 production call sites
and tests the actual header at offsets0..7, narrow/wide colors under ASan/UBSan.
This is alignment/alias-safety evidence, not a rendered hardware-cursor golden.
`proc-ce1654bbcdce` && exit0: sanitizer, both configured builds, real fresh
font-held/bank-held/bank-service (`run-bgdnt0zs`), and four combined debugger→fresh
FILE/INLINE held→explicit continuation/root42/disposal E2Es
(`e2e-milestone-pj_ljquk`). No independent approval or all backend qualification.

Same toolchain/config/header identities as `VGA-PRODUCER-RECURRENCE.md`:
GCC13.3/NASM2.16.01; Lua heavy-debug+Agent+Lua5.4 and native no-debug/no-Agent/
no-Lua, SDL2/FreeType/TTF/printer; no OpenGL/avcodec. Logs retained under card
work root `cursor-{lua,native}.log`. Exact resulting binary SHA256:

* Lua: `6a261524b370337850cdc32bcc153517255054c1900c268737e576d3799a4151`
* native: `d3e1627cf6d09d0280828c7bc259a16a24affe5f6ce30c512ac9d9ed08194160`

The selected diff remains larger than this slice. In particular effective
scan/layout/split/schedule/panning/visibility source-grounded finite disposition
and affected variations still need completion; this document does not transfer
those foundation obligations to parent hardware certification.
