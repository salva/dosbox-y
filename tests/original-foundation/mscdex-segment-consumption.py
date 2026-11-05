#!/usr/bin/env python3
"""Actual MSCDEX native segment codec, reduced memory/callback owners."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mscdex-segment-consumption'
work.mkdir(exist_ok=True)
text=(root/'src/dos/dos_mscdex.cpp').read_text()
body=text[text.index('bool CMscdex::NativeCallbacksIntact()'):text.index('void POD_Save_DOS_Mscdex')]
source=r'''
#include <cstdint>
#include <cassert>
#include <sstream>
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
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
struct CMscdex {uint16_t rootDriverHeaderSeg=0,nativeStrategy=0,nativeInterrupt=0,defaultBufSeg=0;
bool NativeCallbacksIntact() const;void SaveState(std::ostream&);void LoadState(std::istream&);};
#if 1
'''+body+r'''
int main(){
 CMscdex owner;
 for(unsigned installed=0;installed<2;++installed){
  if(installed){owner.rootDriverHeaderSeg=16;owner.defaultBufSeg=32;
   owner.nativeStrategy=1;owner.nativeInterrupt=2;
   CallBack_Handlers[1]=MSCDEX_Strategy_Handler;CallBack_Handlers[2]=MSCDEX_Interrupt_Handler;
   for(unsigned role=0;role<2;++role){unsigned a=16*16+18+5*role;
    ram[a]=0xfe;ram[a+1]=0x38;ram[a+2]=1+role;ram[a+3]=0;ram[a+4]=0xcb;}}
  std::ostringstream output;owner.SaveState(output);assert(output.good());
  auto bytes=output.str();assert(bytes.size()==4);
  {std::istringstream input(bytes);owner.LoadState(input);assert(input.good());}
  for(unsigned n=0;n<bytes.size();++n){
   std::istringstream short_input(bytes.substr(0,n));owner.LoadState(short_input);assert(short_input.fail());
   auto bad=bytes;bad[n]^=1;std::istringstream mismatch(bad);owner.LoadState(mismatch);assert(mismatch.fail());
  }
  if(installed){
   for(unsigned n=0;n<10;++n){unsigned address=16*16+18+n;ram[address]^=1;
    std::ostringstream broken_capture;owner.SaveState(broken_capture);assert(broken_capture.fail());assert(broken_capture.str().empty());
    std::istringstream broken_install(bytes);owner.LoadState(broken_install);assert(broken_install.fail());ram[address]^=1;}
  }
 }
}
'''
(work/'test.cpp').write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-fsanitize=undefined',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual MSCDEX segment codec: present/absent roundtrip; every byte/truncation/binding rejects (reduced UBSan)')
