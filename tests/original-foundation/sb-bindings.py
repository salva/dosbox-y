#!/usr/bin/env python3
"""Actual SB descriptor load against native binding doubles; not sound qualification."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
t=(root/'src/hardware/sblaster.cpp').read_text()
a=t.index('void POD_Load_Sblaster(')
body=t[a:]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'sb-bindings';w.mkdir(exist_ok=True)
s=r'''
#include <cstdint>
#include <sstream>
#include <cassert>
#include <cstring>
using Bitu=uintptr_t;
constexpr size_t MAX_CARDS=2;
struct MixerChannel{unsigned loads=0;void LoadState(std::istream&){++loads;}};
struct DmaChannel{};
DmaChannel channels[8];
DmaChannel* GetDMAChannel(unsigned i){assert(i<8);return &channels[i];}
struct SB_INFO{struct{DmaChannel* chan;}dma;MixerChannel* chan;unsigned value;};
SB_INFO sb[MAX_CARDS];bool test=true;
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
'''+body+r'''
int main(){
 MixerChannel mix[2];SB_INFO live[2]={{{&channels[1]},&mix[0],3},{{&channels[2]},&mix[1],4}};
 SB_INFO incoming[2]={{{nullptr},nullptr,9},{{nullptr},nullptr,10}};
 const char name[32]="SBlaster";
 std::string base(name,sizeof(name));base.append(reinterpret_cast<const char*>(incoming),sizeof(incoming));
 for(unsigned id=0;id<256;++id){for(unsigned c=0;c<2;++c)sb[c]=live[c];
 auto bytes=base;bytes.push_back(static_cast<char>(id));std::istringstream in(bytes);POD_Load_Sblaster(in);
 const bool valid=id<8||id==255;assert(bool(in)==valid);
 for(unsigned c=0;c<2;++c)assert(sb[c].chan==&mix[c]);
 if(valid){assert(sb[0].dma.chan==(id==255?nullptr:&channels[id]));assert(sb[1].dma.chan==&channels[2]);assert(sb[1].value==10);}
 else{assert(sb[0].value==3&&sb[1].value==4);}
 }
 base.push_back(0);
 // Name parsing is outside this descriptor probe; start with a complete name.
 for(size_t n=32;n<base.size();++n){for(unsigned c=0;c<2;++c)sb[c]=live[c];std::istringstream in(base.substr(0,n));POD_Load_Sblaster(in);assert(in.fail());assert(sb[0].value==3&&sb[1].value==4);assert(sb[1].chan==&mix[1]);}
}
'''
(w/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual SB loader: all DMA byte IDs/descriptor truncations preserve native bindings ASan/UBSan PASS; doubles, not sound semantics')
