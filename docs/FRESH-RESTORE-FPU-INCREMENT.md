# Initialized FPU compatibility increment

Author scope: replace one requested-property contribution, CPU `fpu`, with the
initialized CPU owner's enabled/model value. CPU_StateFpuIdentity() returns zero
when absent, otherwise a presence bit plus FPU_ArchitectureType. The config
identity retains all other existing contributions, including cputype/core. No
claim that CPU architecture aliases or prefetch engines are interchangeable.
Disabled FPU model is not observable; enabled 387 and BEST remain distinct.
CPU initialization resolves true/1/auto and explicit model before capture/import.
No raw pointer, path reopen, metadata catalogue or alternate decoder is added.
Config identity intentionally changes; previous config-hash archives reject,
with no migration. Archive representation/owner codec layout remains revision9.

Reproduce in configured heavy-debug SDL2 Lua and no-Lua builds:

```
DOSBOX_TEST_BINARY=/absolute/profile/src/dosbox-x \
  python3 tests/original-foundation/fresh-effective-fpu.py
```

The real SB workload producer requests true; initialized MIXED CPU gives BEST.
Fresh `1` and `auto` restore, stay held, save/observe/delay, then explicitly
step/Run and consume saved SB FIFO to root42/disposal. `false` and `387` reject
Config_Identity, exit1 with disposable-target failure. Producer archive hash
remains unchanged. This is author evidence, not independent approval.

Lua/no-Lua build proc-f123ec8b94b6 exit0; focused tests plus complete Lua four-case
E2E proc-4a21b2e31774 exit0. Evidence directories under card-n-c-k-a work root:
`effective-fpu-4iq4687w`, `effective-fpu-d_6f38_y`, `e2e-milestone-ue1e8ea4`.
Lua binary SHA256677a7c904dd6f1d9f2dc44d84c95137f544f4f5a791eb59ffc8c670d387ddd78;
no-Lua801d3e9eb809709468f3d89769c2c526b680b62f1438f5978321c7e377aabbfe.
Initial proc-d79c3b146186 failed an overly generic diagnostic assertion after
positive cases and correct false exit1; test now checks actual Config_Identity
and disposal diagnostics. It was not a production repair. An apply_patch call
reported no changes; explicit edits subsequently made the actual delta.

Finite affected review request: inspect owning initialized identity, presence/model
separation, unchanged other guards, actual equality positives and mismatch
failure/disposal. Independent prior k-b approval applies only production804,
not this increment. Wider effective hardware/native extents, no-debug, ingress,
close races and documentation reconciliation remain unfinished.
