#!/usr/bin/env python3
"""Selected native-endian cursor loads: alignment/alias safety, not pixel proof."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
source=(root/'src/hardware/vga_draw.cpp').read_text()
assert source.count('VGA_CursorColor<uint16_t>')==12
assert source.count('VGA_CursorColor<uint32_t>')==4
assert '*(uint16_t*)vga.s3.hgc.' not in source
assert '*(uint32_t*)vga.s3.hgc.' not in source
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-cursor-color'
work.mkdir(exist_ok=True)
(work/'test.cpp').write_text(r'''
#include <cassert>
#include <cstdint>
#include "vga_cursor_color.h"
int main(){
 alignas(8) unsigned char colors[16];
 for(unsigned i=0;i<16;++i)colors[i]=i*17;
 for(unsigned offset=0;offset<8;++offset){
  uint16_t narrow;uint32_t wide;
  std::memcpy(&narrow,colors+offset,sizeof(narrow));
  std::memcpy(&wide,colors+offset,sizeof(wide));
  assert(VGA_CursorColor<uint16_t>(colors+offset)==narrow);
  assert(VGA_CursorColor<uint32_t>(colors+offset)==wide);
 }
}
''')
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'src/hardware'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('16 actual cursor load sites audited; native-endian aligned/unaligned ASan/UBSan PASS')
