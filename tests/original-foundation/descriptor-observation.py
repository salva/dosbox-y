#!/usr/bin/env python3
"""Actual GDT/LDT observation body; paging consumption tested separately."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'descriptor-observation-regression'
work.mkdir(exist_ok=True)
text=(root/'src/cpu/cpu.cpp').read_text()
start=text.index('bool GDTDescriptorTable::InspectDescriptor(')
end=text.index('\n}\n',start)+3
source=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
using Bitu=uintptr_t;
struct Descriptor {union {uint32_t fill[2];} saved;};
struct GDTDescriptorTable {
 uint32_t table_base=0x1000,ldt_base=0x2000;
 Bitu table_limit=15,ldt_limit=15;
 bool InspectDescriptor(Bitu,Descriptor&) const;
};
unsigned calls=0;uint32_t seen=0;bool readable=true;
bool PAGING_InspectLinear(uint32_t address,uint32_t length,uint8_t *out){
 ++calls;seen=address;assert(length==8);
 if(!readable)return false;
 for(unsigned i=0;i<8;++i)out[i]=i+1;
 return true;
}
'''+text[start:end]+r'''
int main(){
 GDTDescriptorTable table;Descriptor desc;memset(&desc,0xcc,sizeof desc);
 assert(table.InspectDescriptor(8,desc)&&seen==0x1008&&calls==1);
 assert(desc.saved.fill[0]==0x04030201&&desc.saved.fill[1]==0x08070605);
 assert(table.InspectDescriptor(15,desc)&&seen==0x2008&&calls==2);
 table.ldt_limit=14;assert(!table.InspectDescriptor(12,desc)&&calls==2);
 table.table_limit=7;assert(!table.InspectDescriptor(8,desc)&&calls==2);
 assert(table.InspectDescriptor(0,desc)&&calls==3);
 table.table_base=UINT32_MAX-6;assert(!table.InspectDescriptor(0,desc)&&calls==3);
 table.table_base=UINT32_MAX-7;assert(table.InspectDescriptor(0,desc)&&calls==4);
 table.table_base=0x1000;readable=false;memset(&desc,0xcc,sizeof desc);
 assert(!table.InspectDescriptor(0,desc)&&calls==5);
 assert(desc.saved.fill[0]==0xcccccccc&&desc.saved.fill[1]==0xcccccccc);
}
'''
(work/'observe.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'observe.cpp'),'-o',str(work/'observe')],check=True)
subprocess.run([str(work/'observe')],check=True)
print('GDT/LDT descriptor observation actual-body PASS (paging double; not frontend qualification)')
