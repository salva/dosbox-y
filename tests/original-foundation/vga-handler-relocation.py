#!/usr/bin/env python3
"""Compile actual owner relocation table with distinct stubs; audit live assignments."""
import os
import re
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
src = (root/'src/hardware/vga_draw.cpp').read_text()
a = src.index('static const VGA_Line_Handler saved_vga_line_handlers[]')
b = src.index('\n};', a)+3
table = src[a:b]
names = re.findall(r'\b(?:VGA|EGA|MCGA)_\w+', table.split('{',1)[1])
assert len(names) == len(set(names))
# BIOS logo is a host overlay and intentionally refuses checkpoint, rather than
# installing the destination's unrelated startup handler.
assigned = set(re.findall(r'VGA_DrawLine\s*=\s*((?:VGA|EGA|MCGA)_\w+)', src[:a]))
assert assigned-set(names) == {'VGA_DrawLineBiosLogoOverlay'}, assigned-set(names)
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-handler-relocation'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
    p=Path(d)
    code='#include <cassert>\n#include <cstdint>\nusing Bitu=uintptr_t;\nusing VGA_Line_Handler=uint8_t*(*)(Bitu,Bitu);\n'
    for i,name in enumerate(names):
        code+=f'uint8_t* {name}(Bitu,Bitu){{static uint8_t id={i};return &id;}}\n'
    code+=table+'''\nint main(){
 for(unsigned n=0;n<sizeof(saved_vga_line_handlers)/sizeof(*saved_vga_line_handlers);++n){
  auto original=saved_vga_line_handlers[n];unsigned encoded=255;
  for(unsigned i=0;i<sizeof(saved_vga_line_handlers)/sizeof(*saved_vga_line_handlers);++i)
   if(original==saved_vga_line_handlers[i]){encoded=i;break;}
  assert(encoded==n && saved_vga_line_handlers[encoded]==original);
 }
}\n'''
    (p/'probe.cpp').write_text(code)
    subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'probe.cpp'),'-o',str(p/'probe')],check=True)
    subprocess.run([str(p/'probe')],check=True)
print(f'Actual VGA table {len(names)} handlers/null symmetric identities and live-assignment coverage PASS')
