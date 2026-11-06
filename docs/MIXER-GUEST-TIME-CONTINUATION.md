# Mixer guest-time continuation — 2026-11-06

Selected74b5 `MixerChannel::SavePersistentState` carried lowpass, rate/slew,
interpolation and pending sample history which the original `SaveState` omitted.
The original reset of global pos/done on every device channel import additionally
lost partially mixed output. This is an actual representation gap, not an SDL
waveform certification request.

The one original owner now appends these fields in `SaveState`/`LoadState`;
handler/name/list links stay destination-owned. A single local channel stage
checks rate divisors, filter order, sample/cursor extents and canonical bools
before installing. Global accumulation work ring/cursors, DC/DAC history and
fractional scheduling are traversed once in `SerializeMixer`, not reset per
channel. SDL consumer locking spans traversal; the destination queue and device
identity remain native. No sample generation, callback, register write, clock
advance, replay, Prepared stage or second codec was introduced. Engine25 rejects
earlier archives; representation9 stays unchanged. Host mixer policy remains
config-bound. This is not acceptance of arbitrary malformed global arithmetic
nor acoustic/backend/all-device certification.

`tests/original-foundation/mixer-channel-history.py` extracts the actual codec
and native field declarations. It checks nonzero filter/interpolation/slew and
buffer history, byte-stable re-save, distinct destination native identities,
every truncated prefix, invalid bool and oversized buffer with ASan/UBSan.

`proc-8ba6b2e6a692` exit0: this test, Lua/native/debug-no-Lua builds, diff check,
and adlib-active/speaker-active/cmos-held debugger captures to TWO independent
fresh Lua/no-Lua consumers each passed. Held re-save and explicit continuation
are exercised by the real matrix, directory
`$SAIVAGE_CARD_WORK_ROOT/fresh-root-matrix/run-xexzq0nn`.
These run with nosound=true; native audible SDL output remains unqualified.
Build logs: `$SAIVAGE_CARD_WORK_ROOT/mixer-history-{lua,native,debug-no-lua}.log`.
Existing configured headers/toolchains are unchanged from engine24 handoff.
Final binaries SHA256 Lua `3d539317c5a972bb2fd56aece4dff82d7fc1e00dcbe60680e523f92f5358836f`,
native `e0b025a68f4af652bf23356638544729141f52ee5cde9f09e7afb420ea355fd0`,
debug-no-Lua `502fbf173e8b3920d267f0633bb55115fe204a7881a7a72f2f6a341e98b49616`.

Failed check retained: proc-e49bf97bea0d built, passed active Adlib, then refused
cmos-held Mixer installation. The newly added global fraction-denominator check
incorrectly rejected a legitimate zero-initialized unused fraction. Removed that
invented invariant; real consumption and the existing profile policies remain.
proc-92e03d2e5a29 passed all three cases plus three builds before SDL locking.
proc-bc38f20431ee passed tests but used a nonexistent consumer environment name,
so established only Lua consumption, not cross-profile. Correct variable
DOSBOX_TEST_CONSUMERS passed both consumers proc-803af89d9466, then repeated on
final lock-bearing source above. Two failed patch invocations changed nothing;
selected edit applied the exact inspected source instead.
