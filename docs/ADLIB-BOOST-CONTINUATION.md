# Original Adlib adaptive boost continuation — 2026-11-06

Selected74b5fa198 `Module::FileRecord(AF03)` carries last_opl_write,
single_opl_write_counter, adlib_pcm_boost, adaptive gain and both DC histories.
Original owner at15e2b9be0 already transports mode/register/control/cache,
both Chip timer/poll objects, handler synthesis and mixer channel, but omits
these five mutable boost fields. Real OPL_Write/boost consume them: saving only
the chip changes later write-density threshold/gain/DC sound. Configuration
boost enable/gain maximum are destination policy, not captured mutable history.

Append the missing fields to the existing Module::SaveState/LoadState traversal;
no AF03 framing, duplicate validator, Prepared, register/sample/time replay or
change to ordinary port routing. Engine23→24 rejects incompatible archives;
representation9/API/CLI unchanged. `tests/original-foundation/adlib-boost-history.py`
consumes actual boost implementation and asserts original field traversal,
nonzero gain/DC→fresh reset→restored identical next stereo output plus clock/
count/boost restoration under ASan/UBSan. Reduced function/field evidence, not
an active guest PCM output golden or all backend qualification.

`proc-e9656ec490a8` failed fixture extraction at wrong namespace (included opl.cpp),
so builds did not run. Correct extraction at namespace OPL2;
`proc-e6803b2ce434` && exit0 passes sanitizer/all three builds/diff. Logs card work
`adlib-history-{lua,native,no-lua}.log`, same config/toolchain as handoff.
Current binary SHA256:
- Lua `146d234cbfb90934f1fc757133ecc33d9608a5cee7dcaa8d1577cf49cebefed4`
- native `57bf4dd14c375e38a2280386bb41ebac010d8f7742f1cd2783a6bce76975b0c7`
- debug-no-Lua `73b1bd5a38ed7d144351f9f1b4809aedcc66890b41b4467f64c862a3489ebe3b`

`proc-27a5bf521d72` && exit0 passes four final combined debugger/fresh FILE+INLINE
held→explicit continuation/root42 E2Es (`e2e-milestone-fpmhj7sn`), native no-debug
RTC held/delay/Run/root42/refusal/quit (`native-console-t3j45_ew`), actual exclusive
Interactive/Bridge and full-library Lua regressions. No independent acceptance.
Existing `dbopl-consumption.py` actual chip role/truncation/roundtrip sanitizer
passes on unchanged DBOPL (`proc-aaa333433140`); selected null sentinel versus
original numeric-offset difference remains an investigation, not this repair.
Active guest OPL fixture `fresh-adlib.asm` sets both operators, key-on and1100
actual data writes with PCM detection enabled; capture0200, held/resave/delay then
explicit guest timer/mixer progress and root42. Added adlib-active matrix case;
`proc-75b7a14c61f1` exit0 Lua and debug-no-Lua fresh consumers (`run-3f0hyl2b`).
This checks active owner continuity/no held mutation, not acoustic equality.
Initial direct fresh-startup invocation lacked required archive (`proc-c2ee85783c8c`);
initial fixture label WAIT collided with NASM instruction (`proc-04fcfb1f4b56`);
next producer passed but consumer correctly rejected mismatched boost config
(`proc-ff4633674cfc`, EOF at save after restore). Forward actual ADLIB config to
consumer; no production waiver or force-load. Prior failed evidence preserved.

Full finite source disposition and required current §7/API/CLI/test reconciliation
remain actionable. Historical approved packets untouched.
