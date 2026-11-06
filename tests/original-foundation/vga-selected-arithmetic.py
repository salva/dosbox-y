#!/usr/bin/env python3
"""Differential selected versus actual owner split/visibility arithmetic.

Selected helper sources are pinned Git inputs, not a second production codec.
No guest raster or all-machine certification is implied.
"""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
source=(root/'src/hardware/vga_draw.cpp').read_text()
selected='74b5fa198e619686d506d152b0edcd1d52d5bd0f'
def helper(name):
 text=subprocess.check_output(['git','show',selected+':src/hardware/'+name],cwd=root,text=True)
 return '\n'.join(line for line in text.splitlines() if line!='#include "dosbox.h"')+'\n'
start=source.index('void VGA_Update_SplitLineCompare() {')
end=source.index('\nvoid VGA_DAC_DeferredUpdateColorPalette',start)
split=source[start:end]
start=source.index('\t// default catch all memory access to trigger updates')
end=source.index('\n#if 0//DEBUG',start)
visibility='void ActualVisibility(){\n'+source[start:end]+'\n}\n'
prefix=r'''
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <limits>
using Bitu=unsigned long;
enum {M_EGA,M_LIN4,M_VGA,M_TEXT};
enum {SVGA_None,SVGA_S3Trio};
int svgaCard;bool egavga,pc98;
#define IS_EGAVGA_ARCH egavga
#define IS_PC98_ARCH pc98
struct {
 int mode;
 struct {Bitu line_compare;}config;
 struct {Bitu reg_42;}s3;
 struct {Bitu split_line,render_max,vblank_skip,address,address_add,height,address_line_total;
 unsigned draw_base_planar,draw_base_size;}draw;
}vga;
'''
main=r'''
int main(){
 for(bool s3:{false,true})for(bool interlaced:{false,true})
 for(Bitu compare:{0ul,1ul,2ul,99ul,399ul,1023ul})
 for(Bitu render:{1ul,2ul})for(Bitu skip:{0ul,1ul,17ul,400ul}){
  svgaCard=s3?SVGA_S3Trio:SVGA_None;vga.s3.reg_42=interlaced?0x20:0;
  vga.config.line_compare=compare;vga.draw.render_max=render;vga.draw.vblank_skip=skip;
  VGA_Update_SplitLineCompare();
  assert(vga.draw.split_line==VGA_ResolveSplitCompare(compare,render,s3,interlaced,skip));
 }
 for(bool ega:{false,true})for(bool pc:{false,true})for(int mode:{M_EGA,M_LIN4,M_VGA,M_TEXT})
 for(Bitu address:{0ul,128ul,0xffffff00ul})for(Bitu stride:{0ul,80ul,160ul,0xffffff00ul})
 for(Bitu split:{0ul,199ul,200ul,400ul,std::numeric_limits<Bitu>::max()})
 for(Bitu rows:{1ul,2ul,16ul}){
  // PC98 and EGAVGA are disjoint architecture predicates in production.
  if(ega&&pc)continue;
  egavga=ega;pc98=pc;vga.mode=mode;
  vga.draw.address=address;vga.draw.address_add=stride;vga.draw.split_line=split;
  vga.draw.height=200;vga.draw.address_line_total=rows;
  ActualVisibility();
  auto expected=VGA_ResolveWriteVisibility(ega,mode!=M_TEXT,address,stride,split,200,rows);
  assert(vga.draw.draw_base_planar==expected.base);
  assert(vga.draw.draw_base_size==expected.size);
 }
}
'''
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-selected-arithmetic'
work.mkdir(exist_ok=True)
(work/'test.cpp').write_text(prefix+helper('vga_split_compare.h')+helper('vga_write_visibility.h')+split+visibility+main)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual split/visibility owners vs pinned selected: S3/interlace/skip/split/planar/unsigned-wrap ASan/UBSan PASS')
