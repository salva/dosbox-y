# Tandy destination resource continuation — 2026-11-06

Original owner already includes selected PSG/DAC enabled/last-write/history,
DAC bytes/rate/last sample/transfer-done/mode/control/frequency/amplitude/IRQ,
SN76496 synthesis and both mixer channels. However whole `tandy` POD also
published native channel and DMA addresses. This patch replaces that raw write
with explicit original scalar traversal and canonical boolean import. Local
incoming scalar backing preserves destination mixer-channel pointers; DMA index
resolves only available 8-bit channels0..3. Enabled incomplete DAC without DMA
is refused. No controller callback, synthesis/register/IRQ/time replay or extra
logical codec. Engine28/representation9; older archives incompatible.

`tests/original-foundation/tandy-owner-history.py` extracts actual layout/codec;
ASan/UBSan nonempty sample buffer/rate/last sample/enabled incomplete DMA1,
distinct destination mixer identities, byte-stable resave, every truncated
prefix, invalid DMA7 and missing incomplete DMA refusal pass. Chip/channel are
explicit doubles. Not active Tandy guest/audible output or complete malformed
rate/IRQ certification. `proc-5201ef602e44` exit0 test, Lua/native/no-Lua builds,
diff check. Logs `$SAIVAGE_CARD_WORK_ROOT/tandy-history-{lua,native,debug-no-lua}.log`.
`proc-0b7ab292b4b5` exit0 combined four fresh FILE/INLINE stopped→explicit/root42
E2Es (`e2e-milestone-gwpas2fz`) and native no-debug/no-Agent/no-Lua CMOS console
held/delay→Run/root42/mismatch/quit disposal (`native-console-hrp_u_fw`).
SHA256 Lua `7de951e7ed6ce168bb4637b83dfa0877eeeb5db9c88758c3f2e05c1e5d77cc0a`,
native `f758f5ec7f12e1397558bd821fd2b72029978e488581df5102084cd4b9d34c43`,
no-Lua `a01e20e8d79a400b3afaa2dc65458fe1a58b6e1d8b7ea9dcf24f386679b8e9ff`;
headers/toolchain unchanged. Prior engine27 real production Lua callback/code/file
and Interactive fixture/native SB console passed proc-e9bd902c4197, source-applicable
not current binary evidence. No source/service/publication/cleanup effects.
