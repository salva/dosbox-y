# Mixer signed sample parity — 2026-11-06

Selected74b5fa198 versus029bd65 changes ordinary 8-bit mono/stereo scaling from
left shifting negative signed samples (undefined C++) to signed32 multiplication
by256. Original candidate still retained both shifts. Port the exact selected
expressions in `MixerChannel::loadCurrentSample`; no codec/ABI/engine change,
API/CLI change, scheduling or guest execution. Expected two's-complement sample
values remain unchanged; undefined behavior is removed.

`tests/original-foundation/mixer-scaling.py` consumes the two actual expressions
and checks every signed/unsigned input under ASan/UBSan with no sanitizer recovery.
`proc-a63bbfcfc733` passes that test, original channel truncation/boolean/output
checks, mixer traversal/failure cuts, both builds and diff check. Logs at card
work `mixer-scaling-{lua,native}.log`. Current engine22 binaries SHA256:
Lua `52f8b9ac8f7887564337f972201939f720b021aa969fe43dfecf05d10783ad5b`;
native `2ceb0ecbcfa49e0206df83cec44168a685eddcb943de2092bf07fc51c03c0751`.
Configuration/generated headers/toolchain unchanged from RTC handoff.

`proc-5c7d41039b16` passes four combined FILE/INLINE fresh held→explicit Run/
root42/disposal E2Es (`e2e-milestone-xajkjuf8`) and native RTC held/status/delay/
Run/root42/mismatch+quit disposal (`native-console-92ielw7_`), then fails on a
nonexistent `tests/machine-service/exclusive-runtime.py`. It is not a passing
aggregate. Corrected `proc-ab2bc80f17fa` runs actual
`tests/stopped-continuation/exclusive-runtime.py`, `rpc-runtime.py`, and
`tests/machine-service/lua-production-runtime.py` with && and exit0. RPC actual
stop conditions and production Lua callback/full-library runtime pass;
`rpc-runtime/run-btsvzqfq`, `lua-production-runtime/run-6x73ju8x` retain receipts.
These are author regressions, not independent or whole hardware acceptance.

Reproduce sanitizer: `python3 tests/original-foundation/mixer-scaling.py`.
Actual frontend consumers require DOSBOX_TEST_BINARY pointing to the Lua build;
native-console additionally requires DOSBOX_TEST_NATIVE_BINARY. Retain existing
native console versus debugger transport distinction; no frontend fallback is
introduced by this correction.
