# Ordinary screenshot owner semantic port — 2026-11-06

Source comparison during final finite reconciliation found an actual missing
selected non-save consumer: `hardware.cpp::CAPTURE_AddImage` still contained its
old standalone libpng writer, despite the shared `CapturePNG` owner primitive
already being present for other consumers. The selected map explicitly requires
ordinary screenshot delegation. This was not satisfied by headers or printer
publication tests.

Now the native screenshot branch delegates once to the existing writer with
original source dimensions (undo local DBLH height), pitch, palette, bpp and
DBLW/DBLH, including the Mac non-SDL2 byte layout branch. Writer owns FILE on
all paths and checks encoding/flush/close. The caller logs failure and attempts
to remove incomplete output; failed removal remains explicitly quarantined,
not successful release. Only successful close triggers the existing optional
completion popup. No alternate encoder/publication framework or save codec.
Unchanged video branch still uses its existing conversion scratch and dimensions.

Reproduction: `python3 tests/original-foundation/ordinary-screenshot.py` with
libpng development pkg-config, C++14/ASan/UBSan and card work directory. It
extracts the actual ordinary screenshot branch and uses actual shared writer,
then decodes every pixel for 8/15/16/32-bit with all four doubling combinations.
It verifies dimensions, palette and legacy channel expansion, write/flush failure
through /dev/full, invalid-bpp refusal, incomplete-file removal and absence of
false completion notices. Native file/popup doubles only; this does not claim
physical UI popup/keyboard capture or Mac runtime qualification.

proc-1312607d6f0e exit0: all decoded-pixel/failure sanitizer checks, three builds
and diff check; build logs `ordinary-screenshot-{lua,native,debug-no-lua}.log`.
Engine29/representation9 unchanged, config/header/toolchain as ed293 handoff.
SHA256 Lua `9762f4fe4f84cd1d5874debecc1a8667f63e3a376a5d2ce57f035aee627020d8`,
native `6c7efb5b70f03d87ccd85cfed8f34294ba0f89626652104b9f421ba30b55b250`,
debug-no-Lua `aae3f25057b15e512ef9b2aa70e2e77f05df29515b45beba00985ff2d416b826`.

proc-aea5c8243101: all four combined real fresh FILE/INLINE held→explicit/root42
E2Es passed (`e2e-milestone-ll5sr1xl`); subsequent native check failed because
its producer was erroneously given the no-debug binary. This is NOT a passing
whole chain. Correct producer/native variables in proc-9c61b076e423 exit0 pass
actual no-debug/no-Agent/no-Lua held/delay/status→explicit Run/SB-FIFO/root42 and
mismatch/quit disposal (`native-console-9rztgrhg`), plus diff check.

This is author evidence, not independent acceptance. §7/API/CLI unchanged:
ordinary screenshot remains ordinary screenshot, restore remains fresh-launch,
held until explicit continuation; no implicit rendering/time was added.
