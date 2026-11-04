#!/usr/bin/env python3
"""Execute actual INT10 native-body predicate with reduced callback/RAM doubles."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'int10-body';w.mkdir(exist_ok=True)
t=(root/'src/ints/int10.cpp').read_text()
body=t.split('    static bool nativeBody() {',1)[1].split('    void bindings(',1)[0]
code=r'''
#include <cassert>
#include <cstdint>
using PhysPt=uint32_t; using Bitu=unsigned;
constexpr unsigned CB_MAX=128;
bool IS_PC98_ARCH=false;
Bitu call_10=3;
void INT10_Handler(){} void other(){}
void(*CallBack_Handlers[CB_MAX])()={};
unsigned char ram[4096]={};
PhysPt CALLBACK_PhysPointer(Bitu slot){return slot*32;}
unsigned mem_readb(PhysPt p){assert(p<sizeof(ram));return ram[p];}
unsigned mem_readw(PhysPt p){return mem_readb(p)|(mem_readb(p+1)<<8);}
bool nativeBody(){
'''+body+r'''
int main(){
 CallBack_Handlers[3]=INT10_Handler;
 auto p=CALLBACK_PhysPointer(3);ram[p]=0xfe;ram[p+1]=0x38;ram[p+2]=3;ram[p+4]=0xcf;
 assert(nativeBody());
 for(unsigned i=0;i<5;++i){ram[p+i]^=1;assert(!nativeBody());ram[p+i]^=1;}
 CallBack_Handlers[3]=other;assert(!nativeBody());CallBack_Handlers[3]=INT10_Handler;
 call_10=CB_MAX;assert(!nativeBody());call_10=0;assert(!nativeBody());
 IS_PC98_ARCH=true;assert(nativeBody());
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual INT10 native predicate: slot bounds, handler role, each encoded body byte, PC98 absence passed (reduced doubles, not PC98 runtime)')
