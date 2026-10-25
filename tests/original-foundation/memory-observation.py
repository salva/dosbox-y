#!/usr/bin/env python3
"""Actual physical/linear observation bodies with bounded native-owner doubles."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'memory-observation-regression'
work.mkdir(exist_ok=True)
def body(path,name):
    text=(root/path).read_text(); start=text.index('bool '+name+'('); end=text.index('\n}\n',start)+3
    return text[start:end]
source=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using Bitu=uintptr_t; using PageNum=uint32_t;
constexpr unsigned MEM_PAGESIZE=4096,LINK_START=256;
struct Handler{} ram_page_handler,rom_page_handler,device;
std::vector<uint8_t> backing(16*1024*1024);
uint8_t *MemBase=backing.data();
struct {unsigned handler_pages=4096; Handler *phandlers[4096];} memory;
unsigned MEM_TotalPages(){return 4096;}
struct {bool enabled=false; PageNum firstmb[LINK_START];} paging;
bool do_pse=false;
struct X86PageEntry {
 union {uint32_t load;struct {uint32_t p:1,other:6,ps:1,rest:24;} dirblock;};
 struct Big {PageNum getBase(PageNum page){return 1024+(page&1023);}} dirblock4mb;
};
uint32_t GetPageDirectoryEntryAddr(uint32_t linear){return 0x1000+((linear>>22)*4);}
uint32_t GetPageTableEntryAddr(uint32_t linear,X86PageEntry directory){return (directory.load&~4095u)+((linear>>12)&1023)*4;}
'''+body('src/hardware/memory.cpp','MEM_InspectPhysicalMemory')+body('src/cpu/paging.cpp','PAGING_InspectLinear')+r'''
void word(unsigned at,uint32_t value){memcpy(MemBase+at,&value,4);}
int main(){
 for(auto &h:memory.phandlers)h=&ram_page_handler;
 for(unsigned i=0;i<LINK_START;++i)paging.firstmb[i]=i;
 uint8_t out[4096]; memset(out,0xcc,sizeof out);
 MemBase[4095]=0x42;MemBase[4096]=0x43;
 assert(PAGING_InspectLinear(4095,2,out)&&out[0]==0x42&&out[1]==0x43);
 memory.phandlers[1]=&device;memset(out,0xcc,sizeof out);
 assert(!PAGING_InspectLinear(4095,2,out)&&out[0]==0xcc&&out[1]==0xcc);
 assert(!MEM_InspectPhysicalMemory(4095,2,out)&&out[0]==0xcc);
 memory.phandlers[1]=&rom_page_handler;
 assert(MEM_InspectPhysicalMemory(4095,2,out));
 paging.firstmb[0]=2;MemBase[8192]=0x61;
 assert(PAGING_InspectLinear(0,1,out)&&out[0]==0x61);
 paging.enabled=true;word(0x1000,0x3001);word(0x3000,0x2001);
 assert(PAGING_InspectLinear(0,1,out)&&out[0]==0x61);
 assert(*(uint32_t*)(MemBase+0x1000)==0x3001); // no accessed/dirty writes
 word(0x3004,0);memset(out,0xcc,sizeof out);
 assert(!PAGING_InspectLinear(4095,2,out)&&out[0]==0xcc);
 assert(!PAGING_InspectLinear(UINT32_MAX,2,out));
 assert(!PAGING_InspectLinear(0,4097,out));assert(!PAGING_InspectLinear(0,0,out));
 assert(!PAGING_InspectLinear(0,1,nullptr));
 do_pse=true;word(0x1000,0x81);MemBase[4*1024*1024]=0x73;
 assert(PAGING_InspectLinear(0,1,out)&&out[0]==0x73);
}
'''
(work/'observe.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'observe.cpp'),'-o',str(work/'observe')],check=True)
subprocess.run([str(work/'observe')],check=True)
print('physical/linear observation actual-body regression PASS (owner doubles, not live paging qualification)')
