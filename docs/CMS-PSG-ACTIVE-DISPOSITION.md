# CMS and PSG active synthesis disposition — 2026-11-06

No additional synthesis codec justified. Selected SAA helper carries noise params,
envelope enable/reverse/mode/bits/clock/step, all-channel enable/sync/register,
six amplitude/frequency/octave/enable/envelope/counter/phase channel histories and
two noise clock/counter/frequency/LFSR histories. Original saa1099 Save/Load already
traverses those exact arrays/scalars with base clock/sample rate. Original CMS
owner includes both chips, detect register, base, lastWriteTicks and channel.
Selected SN helper carries volume table/register/last latch/volume/RNG/current
clock/stereo mask/period/count/output/ready; original sn76496 per-field Save/Load
already includes each. Destination variant/feedback/taps/divider/stream identity
remain initialized native policy. Tandy native pointers corrected separately by
`TANDY-NATIVE-CONTINUATION.md`; mixer history by unified original owner.

`tests/original-foundation/mame-active-history.py` compiles actual SAA1099 and
SN76496 classes/codecs/generators (only diagnostic LOG no-op). CMS frequency,
stereo amplitude, octave, tone/noise and envelope are configured; PSG latched tone,
amplitude and noise configured. Each warms256 samples and asserts nonzero output,
captures, imports into fresh initialized chip, compares next256 samples exactly
(CMS both channels; PSG mono). Every truncated prefix refuses. ASan/UBSan pass
proc-6fc55599cef8. No production patch, alternate record or register/time replay
on import. Warm/setup writes are explicit test inputs, not restore operations.
Not exhaustive rates/envelope modes/malformed arithmetic, acoustic/native SDL,
CMS guest port wiring, nor all-ABI certification. Initial fixture compile errors
proc-632bafc948bc (3-argument CMS API/protected PSG methods) and proc-37358f074223
(logging link) were corrected in test subclass/call signature/LOG double only;
production architecture unchanged. Uses engine28 configured Lua headers/GCC13.
