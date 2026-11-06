# Voodoo ordinary callout parity (2026-11-06)

Selected74b5fa198 versus029bd65 contains one ordinary Voodoo lifecycle fix,
separable from its dormant-only save framework: resolved device presence is not
the PCI command bit. Power-on constructs an owner even for `voodoo_card=false`.
Previously that owner allocated a PCI memory callout with no device behind it;
destruction did not release the callout. Port the original selected presence
flag, no-device early release, and destructor release in the original owner.
Enabled software/OpenGL selection and PCI-enable/BAR routing remain unchanged.
No replacement Voodoo codec, Prepared stage, or replay has been imported.

`tests/original-foundation/voodoo-callout.py` extracts the actual allocation,
installation and release functions. Native-callout doubles verify absent device
(no allocation), present+PCI enabled, command disabled (uninstall but retain owned
handle), zero BAR, moved BAR, absent-after-use (release), idempotent explicit
release and a second independent allocation lifetime under ASan/UBSan. This is
not a physical GPU or enabled accelerator integration test. Constructor and
destructor flag wiring were compared directly with selected74b5fa198.

Selected `pci_bus.cpp` adds strict IO handler identities and a dormant-SST
capture/Prepared codec. It has no unrelated PCI runtime fix. The latter only
transports CF8 and matches config/device fields; it is NOT evidence for arbitrary
active PCI/accelerator restore. Selected `voodoo.cpp`'s remaining delta is that
same discarded dormant codec/framework. Active Voodoo restoration remains
unqualified; neither dormant receipts nor this ordinary lifecycle repair certify
it. Exhaustive accelerator/composed qualification belongs to parent scope;
no existing ordinary Voodoo enable path has been removed.

Evidence on engine29/representation9 with only this production change after
ed293c743 (config/header/toolchain unchanged):
* proc-39830c47e3b8 exit0: sanitizer, Lua/native/debug-no-Lua builds,
  diff check. Logs `voodoo-presence-{lua,native,debug-no-lua}.log` in card work.
* proc-e0e021a637bf exit0: four combined real debugger→fresh FILE/INLINE stopped
  observation/save/delay→explicit continuation→root42 disposal E2Es
  (`e2e-milestone-zkti6pex`), actual VESA101 and bank services to fresh consumer
  (`fresh-root-matrix/run-rmlyuopd`).
* Failed proc-c7e00bac852b was the reduced test missing `<cstddef>` for NULL;
  no builds ran behind that failed `&&`. Corrected fixture, not production code.

Binary SHA256 Lua `fe07068a2d2d273a6ac4493cb75bd80c12ea03de1d0684c74d044b3c6c7c2b1e`,
native `7e34488cff0d992d1e81eec75620b5faa1200f47090b021107752e22ad9f15c9`,
debug-no-Lua `a1ce16d0ff9072c0af6415b7a51ac9615d70b53875423fe5b99dfee09ade0943`.

Prior ed293 engine29 all47 root cases passed to TWO fresh Lua/debug-no-Lua
consumers (proc-154819bb76b1 exit0, `fresh-root-matrix/run-h96incvf`), including
active Adlib, RTC, shared handles/search variations, media, XMS/HMA/EMS, input,
AH86/Pause, video, and termination variants. This input preceded the Voodoo
presence change; preserve that applicability rather than claiming all47 rerun.
