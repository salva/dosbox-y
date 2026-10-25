#!/usr/bin/env python3
"""Execute the consumed production scan-stride primitive with model doubles.

This focused arithmetic regression is not rendered-output qualification. It also
runs after extraction into the shared header: the ordinary caller remains the
entry point, so a missing shared dependency fails compilation instead of testing
a handwritten duplicate of its algorithm.
"""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
src = (root/'src/hardware/vga_draw.cpp').read_text()
start = src.index('void VGA_CheckScanLength(void) {')
end = src.index('\nvoid VGA_ActivateHardwareCursor', start)
body = src[start:end]
# Pin the pristine reference to the accepted upstream, not the old selected
# resolver which omitted the 3270 cases. Compare all original switch branches.
reference = subprocess.check_output(['git', 'show',
    '130de209abd6e6fb55b5535f85e2af4f6086ed58:src/hardware/vga_draw.cpp'], cwd=root, text=True)
rs = reference.index('void VGA_CheckScanLength(void) {')
re = reference.index('\nvoid VGA_ActivateHardwareCursor', rs)
reference = reference[rs:re].replace('VGA_CheckScanLength', 'UpstreamScanLength')
prefix = '''#include <cassert>
using Bitu = unsigned long;
enum VGAModes { M_EGA, M_LIN4, M_PACKED4, M_VGA, M_LIN8, M_LIN15,
M_LIN16, M_LIN24, M_LIN32, M_PC98, M_TEXT, M_CGA2, M_CGA16, M_DCGA,
M_AMSTRAD, M_CGA4, M_TANDY2, M_TANDY4, M_TANDY16, M_TANDY_TEXT,
M_HERC_TEXT, M_HERC_GFX };
enum MachineType { MCH_VGA, MCH_CGA, MCH_MCGA, MCH_3270PC };
enum SVGACards { SVGA_None, SVGA_TsengET3K, SVGA_TsengET4K, SVGA_S3Trio };
MachineType machine = MCH_CGA;
SVGACards svgaCard = SVGA_None;
bool egavga = false;
#define IS_EGAVGA_ARCH egavga
struct {
 VGAModes mode;
 struct { bool svga; Bitu bytes_per_scanline; } dosboxig;
 struct { Bitu address_add, blocks, address_line_total; } draw;
 struct { Bitu scan_len, addr_shift; } config;
} vga;
'''
tests = '''
int main() {
 for (int integrated=0; integrated<2; ++integrated)
 for (int m=-1; m<=M_HERC_GFX+1; ++m)
 for (int model=MCH_VGA; model<=MCH_3270PC; ++model)
 for (int card=SVGA_None; card<=SVGA_S3Trio; ++card)
 for (int shift=0; shift<=2; ++shift)
 for (int arch=0; arch<2; ++arch) {
  vga.dosboxig.svga=integrated; vga.dosboxig.bytes_per_scanline=1234;
  vga.mode=static_cast<VGAModes>(m); machine=static_cast<MachineType>(model);
  svgaCard=static_cast<SVGACards>(card); egavga=arch;
  vga.config.scan_len=13; vga.config.addr_shift=shift;
  vga.draw.blocks=45; vga.draw.address_line_total=8;
  UpstreamScanLength(); auto expected=vga.draw.address_add;
  VGA_CheckScanLength(); assert(vga.draw.address_add==expected);
 }
 egavga=false; svgaCard=SVGA_None; vga.dosboxig.svga=false;
 vga.draw.blocks=45; vga.draw.address_line_total=8;
 vga.config.scan_len=7; vga.config.addr_shift=1;
 for (auto mode : {M_CGA2, M_CGA4}) {
  vga.mode=mode; machine=MCH_3270PC; VGA_CheckScanLength();
  assert(vga.draw.address_add==360);
  vga.draw.address_line_total=16; VGA_CheckScanLength();
  assert(vga.draw.address_add==720);
  machine=MCH_CGA; VGA_CheckScanLength(); assert(vga.draw.address_add==45);
  vga.draw.address_line_total=8;
 }
 vga.mode=M_CGA2; egavga=true; VGA_CheckScanLength();
 assert(vga.draw.address_add==28);
 vga.mode=M_EGA; VGA_CheckScanLength(); assert(vga.draw.address_add==112);
 vga.mode=M_VGA; machine=MCH_MCGA; VGA_CheckScanLength();
 assert(vga.draw.address_add==180);
 vga.dosboxig.svga=true; vga.dosboxig.bytes_per_scanline=1234;
 VGA_CheckScanLength(); assert(vga.draw.address_add==1234);
}
'''
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'scan-stride-regression'
work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as tmp:
    path = Path(tmp)
    helper = (root/'src/hardware/vga_scan_addressing.h').read_text()
    helper = '\n'.join(line for line in helper.splitlines() if not line.startswith('#include'))
    (path/'scan.cpp').write_text('#include <initializer_list>\n'+prefix+helper+'\n'+reference+'\n'+body+tests)
    subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror', str(path/'scan.cpp'), '-o', str(path/'scan')], check=True)
    subprocess.run([str(path/'scan')], check=True)
print('PASS consumed VGA_CheckScanLength: CGA2/CGA4 3270 stride, CGA, EGA, MCGA, integrated')
