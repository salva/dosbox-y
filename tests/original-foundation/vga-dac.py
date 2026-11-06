#!/usr/bin/env python3
"""Production DAC write/send/owner codec: pending triplet and applied banks."""
import os
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=(root/'src/hardware/vga_dac.cpp').read_text()
def body(name):
 a=src.index(name); b=src.index('{',a); depth=1;c=b+1
 while depth:
  if src[c]=='{':depth+=1
  if src[c]=='}':depth-=1
  c+=1
 return src[a:c]
functions='\n'.join(body(n) for n in ('static inline uint8_t dacexpand(', 'static void VGA_DAC_SendColor(', 'void write_p3c9(', 'void POD_Save_VGA_Dac(', 'void POD_Load_VGA_Dac('))
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-dac'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p=Path(d)
 (p/'t.cpp').write_text('''#include <cassert>
#include <sstream>
#include <cstdint>
#include <cstring>
#include "vga_applied_palette.h"
using Bitu=unsigned;
enum {M_CGA16,M_VGA,MCH_MCGA};
struct Dac {struct {uint8_t red,green,blue;} rgb[256]{};uint32_t xlat32[256]{};uint16_t xlat16[256]{};unsigned hidac_counter=0,pel_index=0,write_index=0,read_index=0;};
struct {Dac dac;struct {bool vga_dac_lockout=false;} dosboxig;struct {unsigned mcga_mode_control=0;} other;int mode=M_VGA;} vga;
int machine=0;bool vga_8bit_dac=true,enable_vga_8bit_dac=true,vga_palette_update_on_full_load=true,vga_render_on_demand=false;
unsigned GFX_bpp=32,GFX_Rshift=16,GFX_Gshift=8,GFX_Bshift=0;uint32_t GFX_Amask=0;
unsigned VGA_DAC_DeferredUpdate=0,render_calls=0;
unsigned char tmp_dac[3]={};
VGA_AppliedPalette applied_palette;
void VGA_DAC_RebuildAppliedLookupTables(){applied_palette.rebuild(vga.dac,GFX_Rshift,GFX_Gshift,GFX_Bshift,GFX_Amask);}
void RENDER_SetPal(uint8_t,uint8_t,uint8_t,uint8_t){++render_calls;}
void VGA_RenderOnDemandUpTo(){assert(false);}
#define WRITE_POD(p,x) stream.write(reinterpret_cast<const char*>(p),sizeof(x))
#define READ_POD(p,x) stream.read(reinterpret_cast<char*>(p),sizeof(x))
'''+functions+'''
int main(){
 // Two different already-applied banks, then a newer deferred hardware RGB.
 GFX_bpp=16;GFX_Rshift=11;GFX_Gshift=5;
 vga.dac.rgb[7]={80,100,120};VGA_DAC_SendColor(7,7);
 auto narrow=vga.dac.xlat16[7];
 GFX_bpp=32;GFX_Rshift=16;GFX_Gshift=8;
 vga.dac.rgb[7]={130,140,150};VGA_DAC_SendColor(7,7);
 auto wide=vga.dac.xlat32[7];assert(narrow!=0 && wide==0x828c96);
 vga.dac.rgb[7]={1,2,3};VGA_DAC_DeferredUpdate=4;
 // Partial full-load triplet cannot be recovered from rgb[].
 vga.dac.write_index=9;write_p3c9(0,41,1);write_p3c9(0,42,1);
 std::stringstream out;POD_Save_VGA_Dac(out);auto bytes=out.str();
 std::memset(tmp_dac,0,3);VGA_DAC_DeferredUpdate=0;applied_palette={};
 vga.dac={};unsigned calls=render_calls;
 std::stringstream in(bytes);POD_Load_VGA_Dac(in);assert(in);
 assert(render_calls==calls && VGA_DAC_DeferredUpdate==4);
 assert(vga.dac.xlat16[7]==uint16_t((uint32_t(80>>3)<<16)|(uint32_t(100>>2)<<8)|(120>>3)) && vga.dac.xlat32[7]==wide);
 assert(vga.dac.rgb[7].red==1 && vga.dac.pel_index==2);
 write_p3c9(0,43,1);
 assert(vga.dac.rgb[9].red==41 && vga.dac.rgb[9].green==42 && vga.dac.rgb[9].blue==43);
 assert(VGA_DAC_DeferredUpdate==5 && render_calls==calls);
 // Different destination packing rebuilds applied history, not deferred RGB.
 GFX_Rshift=0;GFX_Bshift=16;std::stringstream swapped(bytes);POD_Load_VGA_Dac(swapped);
 assert(swapped && vga.dac.xlat32[7]==0x968c82 && render_calls==calls);
 for(unsigned n=0;n<bytes.size();++n){std::stringstream short_in(bytes.substr(0,n));POD_Load_VGA_Dac(short_in);assert(!short_in);}
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined','-I'+str(root/'src/hardware'),str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('Actual DAC pending triplet/deferred counter + independent applied banks/repack/no Render delivery/truncations ASan/UBSan PASS')
