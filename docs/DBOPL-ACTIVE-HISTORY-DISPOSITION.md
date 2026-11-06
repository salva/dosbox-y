# DBOPL bounded selected disposition — 2026-11-06

Original owner already transports the entire chip's mutable operator envelopes,
phases, feedback, noise and LFO plus table-relative wave bases and explicit
volume/synth roles. Selected74b5fa198 uses a null waveBase sentinel and a narrower
allowed-offset catalogue. Constructor null alone is not a missing initialized
continuation: `Handler::Init → Chip::Setup` writes every register in OPL3 then
OPL2, including every E0, replacing operator constructor null with WaveTable
bases. No production change justified for a constructor-only null archive.
This is not approval of arbitrary malformed aligned wave offsets or every ABI.

Extended actual `dbopl-consumption.py`: eight active OPL3 waveforms, both
operators, nonzero synthesized samples and advancing envelopes; capture after
256 frames, fresh initialized owner import then next256 stereo frames byte
comparison. Existing thirteen malformed/truncated role/extent/LFO cases remain.
`proc-1b0d394c95ff` exit0 ASan/UBSan. Exact producer source engine24 production
3a8f6485eb37c7c507f2930446899d414fe3edb5; no layout or binary change. Two attempted
apply_patch calls reported no changes; exact edit succeeded. Test uses same
actual dbopl.cpp with mixer stubs, not full acoustic/host output certification.

Current configured Lua→debug-no-Lua engine24 consumer variations cmos-held,
font-held,bank-held,shared-file,speaker-active passed (`run-4v535ulq`) plus actual
RPC runtime (`rpc-runtime/run-bev35t4b`), `proc-c847939bc07f` && exit0. Final
combined FILE/INLINE/native/exclusive/Lua evidence remains in
ADLIB-BOOST-CONTINUATION.md. No independent acceptance or whole-foundation claim.
