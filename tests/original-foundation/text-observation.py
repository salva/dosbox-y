#!/usr/bin/env python3
"""Actual text observation body with owner doubles; not live rendering proof."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'text-observation-regression'
work.mkdir(exist_ok=True)
text = (root/'src/hardware/vga_draw.cpp').read_text()
body = text[text.index('bool VGA_CopyTextSnapshot('):text.index('static uint32_t FontMask[2]')]
source = r'''
#include "native_stop_conditions.h"
#include <cassert>
#include <array>
#define USE_TTF 1
bool IS_DOSV=false, IS_JEGA_ARCH=false;
struct { bool inUse=false; } ttf;
enum { MCH_VGA, MCH_CGA, M_TEXT, M_OTHER };
int machine=MCH_VGA;
struct Mode { int mode=3; } mode;
Mode *CurMode=&mode;
uint64_t PIC_Ticks=42;
struct { uint16_t loaded_codepage=437; } dos;
struct {
 int mode=M_TEXT;
 struct { unsigned blocks=80,address_add=160,height=400,address_line_total=16;
 unsigned panning=0,bytes_skip=0,vblank_skip=0,split_line=400;
 uint8_t *linear_base=nullptr; unsigned linear_mask=65535; } draw;
 struct { unsigned addr_shift=1,hlines_skip=0,real_start=0; } config;
} vga;
''' + body + r'''
int main() {
 std::array<uint8_t,65536> backing{};
 for(unsigned i=0;i<backing.size();++i)backing[i]=uint8_t(i*7);
 const auto original=backing;
 vga.draw.linear_base=backing.data();
 NativeDebug::TextRegion region{2,3,4,2};
 NativeDebug::TextSnapshot image;
 assert(VGA_CopyTextSnapshot(region,image));
 assert(image.cells.size()==16 && image.tick==42 && image.dos_codepage==437);
 assert(image.start==0 && image.stride==160);
 for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x) {
  unsigned at=((y+3)*160*4+(x+2)*8)&65535;
  assert(image.cells[2*(y*4+x)]==backing[at]);
  assert(image.cells[2*(y*4+x)+1]==backing[at+1]);
 }
 vga.config.real_start=8191; // Wrap near end of planar backing.
 assert(VGA_CopyTextSnapshot({0,0,80,25},image));
 assert(image.cells.size()==4000 && image.start==16382);
 const auto saved=image.cells;
 auto refused=[&]() { assert(!VGA_CopyTextSnapshot(region,image)); assert(image.cells==saved); };
 machine=MCH_CGA;refused();machine=MCH_VGA;
 ttf.inUse=true;refused();ttf.inUse=false;
 IS_DOSV=true;refused();IS_DOSV=false;
 IS_JEGA_ARCH=true;refused();IS_JEGA_ARCH=false;
 CurMode=nullptr;refused();CurMode=&mode;
 vga.draw.panning=1;refused();vga.draw.panning=0;
 vga.draw.address_add=80;refused();vga.draw.address_add=160;
 vga.draw.split_line=399;refused();vga.draw.split_line=400;
 vga.draw.linear_base=nullptr;refused();vga.draw.linear_base=backing.data();
 assert(!VGA_CopyTextSnapshot({79,24,2,1},image) && image.cells==saved);
 assert(backing==original);
}
'''
(work/'text.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'include'),str(work/'text.cpp'),'-o',str(work/'text')],check=True)
subprocess.run([str(work/'text')],check=True)
print('actual VGA text snapshot body sanitizer PASS; owner doubles, not live rendering/lifetime qualification')
