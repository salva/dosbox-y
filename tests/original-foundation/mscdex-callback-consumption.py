#!/usr/bin/env python3
"""Actual MSCDEX original strategy/interrupt binding check, reduced doubles."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mscdex-callback-consumption'
work.mkdir(exist_ok=True)
text=(root/'src/dos/dos_mscdex.cpp').read_text()
body=text[text.index('bool CMscdex::NativeCallbacksIntact()'):].split('#endif',1)[0]
source=r'''
#include <cstdint>
#include <cassert>
using PhysPt=unsigned;
using CallBack_Handler=unsigned(*)();
constexpr unsigned CB_MAX=128;
unsigned MSCDEX_Strategy_Handler(){return 0;}
unsigned MSCDEX_Interrupt_Handler(){return 1;}
CallBack_Handler CallBack_Handlers[CB_MAX]={};
uint8_t ram[4096]={};
struct DOS_DeviceHeader {struct sDeviceHeader {uint8_t bytes[18];};};
PhysPt PhysMake(unsigned seg,unsigned off){return seg*16+off;}
uint8_t mem_readb(unsigned a){assert(a<sizeof(ram));return ram[a];}
uint16_t mem_readw(unsigned a){return mem_readb(a)|(unsigned(mem_readb(a+1))<<8);}
struct CMscdex {uint16_t rootDriverHeaderSeg=0,nativeStrategy=0,nativeInterrupt=0;
bool NativeCallbacksIntact() const;};
'''+body+r'''
int main(){
 CMscdex owner;assert(owner.NativeCallbacksIntact());
 owner.nativeStrategy=1;assert(!owner.NativeCallbacksIntact());
 owner.rootDriverHeaderSeg=16;owner.nativeInterrupt=2;
 CallBack_Handlers[1]=MSCDEX_Strategy_Handler;
 CallBack_Handlers[2]=MSCDEX_Interrupt_Handler;
 unsigned base=16*16+18;
 for(unsigned role=0;role<2;++role){unsigned a=base+5*role;
 ram[a]=0xfe;ram[a+1]=0x38;ram[a+2]=1+role;ram[a+3]=0;ram[a+4]=0xcb;}
 assert(owner.NativeCallbacksIntact());
 for(unsigned i=0;i<10;++i){ram[base+i]^=1;assert(!owner.NativeCallbacksIntact());ram[base+i]^=1;}
 CallBack_Handlers[1]=MSCDEX_Interrupt_Handler;assert(!owner.NativeCallbacksIntact());
 CallBack_Handlers[1]=MSCDEX_Strategy_Handler;
 owner.nativeInterrupt=CB_MAX;assert(!owner.NativeCallbacksIntact());
 owner.nativeInterrupt=0;assert(!owner.NativeCallbacksIntact());
}
'''
(work/'test.cpp').write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-fsanitize=undefined',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual MSCDEX callback role check: absent/slot/handler/every controlled byte reject (reduced UBSan)')
