#!/usr/bin/env python3
"""Consume actual VGA aperture handlers with independent read/write banks.

Reduced ASan/UBSan owner test, not hardware/rendered-output certification.
"""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
source = (root / 'src/hardware/vga_memory.cpp').read_text()
body = source[source.index('class VGA_ChainedVGA_Handler :'):source.index('\n#include <stdio.h>', source.index('class VGA_ChainedVGA_Handler :'))]
prefix = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
using PhysPt=uint32_t;
#define INLINE inline
#define PFLAG_NOCODE 1
struct PageHandler {
 explicit PageHandler(int){}
 virtual uint8_t readb(PhysPt)=0; virtual uint16_t readw(PhysPt)=0;
 virtual uint32_t readd(PhysPt)=0; virtual void writeb(PhysPt,uint8_t)=0;
 virtual void writew(PhysPt,uint16_t)=0; virtual void writed(PhysPt,uint32_t)=0;
};
struct { uint32_t mask=0xffff; } vgapages;
alignas(16) uint8_t memory[1<<20];
struct {
 struct { uint8_t *linear=memory; uint32_t memmask=(1<<20)-1; }mem;
 struct { uint32_t bank_read_full=0, bank_write_full=0; }svga;
 struct {uint8_t read_map_select=0; uint32_t full_not_map_mask=0,full_map_mask=~0u;}config;
}vga;
uint32_t ExpandTable[256];
PhysPt PAGING_GetPhysicalAddress(PhysPt a){return a;}
void VGAMEM_USEC_read_delay(){} void VGAMEM_USEC_write_delay(){}
void vga_vram_write_trigger_update(){} void vga_vram_write_trigger_update_planar_mem(PhysPt){}
uint8_t VGA_Generic_Read_Handler(PhysPt a,PhysPt,uint8_t){return memory[a*4];}
template<bool C> void VGA_Generic_Write_Handler(PhysPt a,PhysPt,uint8_t v){memory[a*4]=v;}
'''
main = r'''
template<class H> void check(){
 H h;
 for(unsigned rb:{0u,0x1000u,0x3000u})for(unsigned wb:{0u,0x2000u,0x4000u}){
  vga.svga.bank_read_full=rb; vga.svga.bank_write_full=wb;
  for(unsigned size:{1u,2u,4u})for(unsigned a:{0u,1u,2u,3u,15u}){
   memset(memory,0,sizeof(memory));
   // Record reads with the read bank only. A write may not touch those bytes
   // unless the banks deliberately alias.
   if(size==1)h.writeb(a,0x5a); else if(size==2)h.writew(a,0x5a5a);else h.writed(a,0x5a5a5a5a);
   if(rb!=wb){assert(h.readb(a)==0);assert(h.readw(a)==0);assert(h.readd(a)==0);}
   vga.svga.bank_read_full=wb;
   assert(h.readb(a)==0x5a);
   if(size>=2)assert(h.readw(a)==0x5a5a);
   if(size==4)assert(h.readd(a)==0x5a5a5a5a);
   vga.svga.bank_read_full=rb;
  }
 }
}
int main(){for(unsigned i=0;i<256;i++)ExpandTable[i]=i*0x01010101u;
 check<VGA_ChainedVGA_Handler>();check<VGA_ChainedVGA_Slow_Handler>();
 check<VGA_ET4000_ChainedVGA_Slow_Handler>();check<VGA_UnchainedVGA_Handler>();
 check<VGA_UnchainedVGA_Fast_Handler>();}
'''
# initializer_list is needed by the finite variation loops.
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'vga-bank-regression'
work.mkdir(exist_ok=True)
(work/'test.cpp').write_text('#include <initializer_list>\n'+prefix+body+main)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Five production VGA handlers: separate/alias banks, byte/word/dword, aligned/unaligned ASan/UBSan PASS')
