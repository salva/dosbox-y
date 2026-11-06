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
start=source.index('\t\tvga.draw.address_line_total=(vga.crtc.maximum_scan_line&0x1fu)+1u;')
end=source.index('\n\t//Set the bpp',start)
policy='void ActualPolicy(){\n'+source[start:end]+'\n}\n'
# The extracted region closes the enclosing EGAVGA setup else-if.
policy=policy.replace('\n\t}\n\n\tif (!vga.draw.doublescan_effect)', '\n\n\tif (!vga.draw.doublescan_effect)',1)
start=source.index('\t// Vertical blanking tricks')
end=source.index('\n\tvga.draw.vblank_skip',start)
blank='void ActualBlank(){\n'+source[start:end]+'\n}\n'
start=source.index('\t// default catch all memory access to trigger updates')
end=source.index('\n#if 0//DEBUG',start)
visibility='void ActualVisibility(){\n'+source[start:end]+'\n}\n'
prefix=r'''
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <limits>
using Bitu=unsigned long;
enum {M_EGA,M_LIN4,M_VGA,M_TEXT,M_CGA16,M_CGA2,M_CGA4,M_DCGA,M_PC98};
bool compatible;
bool RENDER_IsScalerCompatibleWithDoublescan(){return compatible;}
Bitu height;
enum {SVGA_None,SVGA_S3Trio};
int svgaCard;bool egavga,pc98,vga_arch,ignore_vblank_wraparound;
Bitu vblank_skip,vdend,vtotal,vbstart,vbend;
#define IS_VGA_ARCH vga_arch
#define LOG(...) [](const char*, ...){ }
#define LOG_VGA 0
#define LOG_WARN 0
#define IS_EGAVGA_ARCH egavga
#define IS_PC98_ARCH pc98
struct {
 int mode;
 struct {Bitu line_compare;}config;
 struct {Bitu maximum_scan_line;}crtc;
 struct {Bitu reg_42;}s3;
 struct {bool doublescan_set,doublescan_effect;
 Bitu split_line,render_max,vblank_skip,address,address_add,height,address_line_total;
 unsigned draw_base_planar,draw_base_size;}draw;
}vga;
'''
main=r'''
int main(){
 for(bool requested:{false,true})for(bool compat:{false,true})for(bool arch:{false,true})
 for(int mode:{M_TEXT,M_VGA,M_EGA,M_CGA2})for(Bitu maximum:{0ul,1ul,15ul,31ul,128ul,129ul,159ul}){
  compatible=compat;vga_arch=arch;vga.mode=mode;vga.crtc.maximum_scan_line=maximum;
  vga.draw.doublescan_set=requested;vga.draw.render_max=1;height=400;
  ActualPolicy();
  Bitu rows=((maximum&31)+1)*((maximum&128)?2:1);
  auto expected=VGA_ResolveScanPolicy(requested,compat,rows,arch&&mode!=M_TEXT);
  assert(vga.draw.address_line_total==rows);
  assert(vga.draw.doublescan_effect==expected.doublescan);
  assert(height==(expected.halve_height?200ul:400ul));
  assert(vga.draw.render_max==(expected.doublescan?1ul:2ul));
 }
 for(bool arch:{false,true})for(bool pc:{false,true})for(bool ignore:{false,true})
 for(Bitu total:{200ul,400ul})for(Bitu display:{199ul,200ul,400ul})
 for(Bitu start:{0ul,1ul,17ul,199ul,400ul})for(Bitu end:{0ul,1ul,20ul,201ul,385ul,401ul,512ul}){
  vga_arch=arch;pc98=pc;ignore_vblank_wraparound=ignore;
  vtotal=total;vdend=display;vbstart=start;vbend=end;
  ActualBlank();auto expected=VGA_ResolveBlankGeometry(display,total,start,end,(arch||pc)&&!ignore);
  assert(vdend==expected.display_end && vblank_skip==expected.skip);
 }
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
(work/'test.cpp').write_text(prefix+helper('vga_split_compare.h')+helper('vga_write_visibility.h')+helper('vga_blank_geometry.h')+helper('vga_scan_policy.h')+split+visibility+blank+policy+main)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual split/visibility/blank/scan-policy owners vs pinned selected: S3/interlace/skip/planar/unsigned-wrap/doublescan ASan/UBSan PASS')
