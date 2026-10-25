#!/usr/bin/env python3
"""RPC segmented resolver consumption of the real descriptor observer."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'rpc-segment-observation'
work.mkdir(exist_ok=True)
def body(path, signature):
    text = (root/path).read_text()
    start = text.index(signature)
    return text[start:text.index('\n}\n', start)+3]
source = r'''
#include <cassert>
#include <cstdint>
#include <limits>
#include <cstddef>
#include <string>
using Bitu=uintptr_t;
struct Descriptor {
 union { uint32_t fill[2]; struct {
 uint32_t lim0:16,base0:16,base1:8,type:5,dpl:2,p:1,lim1:4,avl:1,res:1,big:1,g:1,base2:8;
 } seg; } saved;
 unsigned Type() const {return saved.seg.type;}
 bool Big() const {return saved.seg.big;}
 bool GetExpandDown() const {return (Type() & 0x1c)==0x14;}
 uint32_t GetLimit() const {uint32_t n=saved.seg.lim0 | (saved.seg.lim1<<16);return saved.seg.g?(n<<12)|0xfff:n;}
 uint32_t GetBase() const {return saved.seg.base0 | (saved.seg.base1<<16) | (saved.seg.base2<<24);}
};
struct GDTDescriptorTable {
 uint32_t table_base=0x1000,ldt_base=0x2000;
 Bitu table_limit=15,ldt_limit=15;
 bool InspectDescriptor(Bitu,Descriptor&) const;
};
struct {bool pmode=false;GDTDescriptorTable gdt;} cpu;
uint32_t reg_flags=0;constexpr uint32_t FLAG_VM=0x20000;
struct MemoryAddress {uint16_t segment;uint32_t offset;};
struct MemoryAccessError {std::string reason;uint32_t failing_offset=0;};
void SetAccessError(MemoryAccessError *e,const char *s,uint32_t off){if(e){e->reason=s;e->failing_offset=off;}}
Descriptor observed;bool readable=true;unsigned reads=0;
bool PAGING_InspectLinear(uint32_t address,uint32_t length,uint8_t *out){
 ++reads;assert(address==0x1008||address==0x2008);assert(length==8);
 if(!readable)return false;
 for(unsigned i=0;i<8;++i)out[i]=observed.saved.fill[i/4]>>(8*(i%4));
 return true;
}
'''
source += body(Path('src/cpu/cpu.cpp'),'bool GDTDescriptorTable::InspectDescriptor(')
source += body(Path('src/agent/debugger/debugger_adapter.cpp'),'bool ResolveSegmentedByte(')
source += r'''
int main(){
 MemoryAddress a{8,0x12345};MemoryAccessError e;uint32_t out=0xdeadbeef;
 assert(ResolveSegmentedByte(a,0,&out,&e)&&out==0x23c5&&reads==0);
 cpu.pmode=true;reg_flags=FLAG_VM;
 assert(ResolveSegmentedByte(a,0,&out,&e)&&reads==0);
 reg_flags=0;observed.saved.fill[0]=0;observed.saved.fill[1]=0;
 observed.saved.seg.type=0x12;observed.saved.seg.p=1;
 observed.saved.seg.lim0=0xffff;observed.saved.seg.base0=0x100;
 assert(ResolveSegmentedByte(a,0,&out,&e)&&out==0x2445&&reads==1);
 observed.saved.seg.big=1;
 out=0xdeadbeef;assert(!ResolveSegmentedByte(a,0,&out,&e)&&out==0xdeadbeef&&e.reason=="segment_limit");
 a.offset=0xffff;assert(ResolveSegmentedByte(a,0,&out,&e)&&out==0x100ff);
 observed.saved.seg.type=0x16;
 assert(!ResolveSegmentedByte(a,0,&out,&e)&&e.reason=="segment_limit");
 a.offset=0x10000;assert(ResolveSegmentedByte(a,0,&out,&e)&&out==0x10100);
 observed.saved.seg.p=0;assert(!ResolveSegmentedByte(a,0,&out,&e)&&e.reason=="invalid_selector");
 observed.saved.seg.p=1;readable=false;
 assert(!ResolveSegmentedByte(a,0,&out,&e)&&e.reason=="invalid_selector");
 readable=true;a.segment=12;assert(ResolveSegmentedByte(a,0,&out,&e));
 a.offset=UINT32_MAX;assert(!ResolveSegmentedByte(a,1,&out,&e)&&e.reason=="address_overflow");
 observed.saved.seg.base2=0xff;observed.saved.seg.base1=0xff;observed.saved.seg.base0=0xffff;
 a.offset=0x10000;assert(!ResolveSegmentedByte(a,0,&out,&e)&&e.reason=="address_overflow");
}
'''
(work/'observe.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'observe.cpp'),'-o',str(work/'observe')],check=True)
subprocess.run([str(work/'observe')],check=True)
print('RPC segmented resolver + descriptor observation PASS (CPU/paging doubles, not live RPC qualification)')
