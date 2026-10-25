#!/usr/bin/env python3
"""Actual SB save/load against native binding doubles; not sound qualification."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
t=(root/'src/hardware/sblaster.cpp').read_text()
body=t[t.index('void POD_Save_Sblaster('):]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'sb-bindings';w.mkdir(exist_ok=True)
s=r'''
#include <cstdint>
#include <sstream>
#include <cassert>
#include <cstring>
constexpr size_t MAX_CARDS=2;
struct MixerChannel{unsigned value=0;void LoadState(std::istream& s){s.read(reinterpret_cast<char*>(&value),sizeof(value));}void SaveState(std::ostream& s){s.write(reinterpret_cast<const char*>(&value),sizeof(value));}};
struct DmaChannel{};
DmaChannel channels[8];bool missing=false;
DmaChannel* GetDMAChannel(unsigned i){assert(i<8);return missing&&i==7?nullptr:&channels[i];}
struct SB_INFO{struct{DmaChannel* chan;}dma;MixerChannel* chan;unsigned value;};
SB_INFO sb[MAX_CARDS];
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
'''+body+r'''
int main(){
 MixerChannel mix[2],fresh[2];
 for(unsigned mask=0;mask<4;++mask){
  for(unsigned c=0;c<2;++c){sb[c]={{{nullptr}},nullptr,0};sb[c].dma.chan=&channels[c+1];sb[c].chan=(mask&(1<<c))?&mix[c]:nullptr;sb[c].value=9+c;mix[c].value=31+c;}
  std::ostringstream out;POD_Save_Sblaster(out);assert(out);const auto bytes=out.str();
  for(unsigned c=0;c<2;++c){sb[c].chan=(mask&(1<<c))?&fresh[c]:nullptr;sb[c].value=0;sb[c].dma.chan=nullptr;fresh[c].value=0;}
  std::istringstream in(bytes);POD_Load_Sblaster(in);assert(in);
  for(unsigned c=0;c<2;++c)if(mask&(1<<c)){assert(sb[c].chan==&fresh[c]&&sb[c].dma.chan==&channels[c+1]&&sb[c].value==9+c&&fresh[c].value==31+c);}
  for(size_t n=0;n<bytes.size();++n){std::istringstream truncated(bytes.substr(0,n));POD_Load_Sblaster(truncated);assert(truncated.fail());}
 }
 // Build a one-card stream and exercise every consumed relocation byte.
 sb[0]={{{&channels[1]}},&mix[0],9};sb[1]={};std::ostringstream out;POD_Save_Sblaster(out);assert(out);const auto base=out.str();
 const size_t offset=32+1+sizeof(SB_INFO);
 for(unsigned id=0;id<256;++id){auto bytes=base;bytes[offset]=static_cast<char>(id);sb[0].chan=&fresh[0];sb[0].value=3;std::istringstream in(bytes);POD_Load_Sblaster(in);const bool valid=id<8||id==255;assert(bool(in)==valid);assert(sb[0].chan==&fresh[0]);if(valid)assert(sb[0].dma.chan==(id==255?nullptr:&channels[id]));else assert(sb[0].value==3);}
 auto bytes=base;bytes[offset]=7;missing=true;std::istringstream unavailable(bytes);POD_Load_Sblaster(unavailable);assert(unavailable.fail());missing=false;
 bytes=base;bytes[32]=2;std::istringstream invalid(bytes);POD_Load_Sblaster(invalid);assert(invalid.fail());
 sb[0].chan=nullptr;std::istringstream mismatch(base);POD_Load_Sblaster(mismatch);assert(mismatch.fail());
 sb[0].chan=&mix[0];sb[0].dma.chan=nullptr;missing=true;std::ostringstream nullout;POD_Save_Sblaster(nullout);assert(nullout);std::istringstream nullin(nullout.str());POD_Load_Sblaster(nullin);assert(nullin && sb[0].dma.chan==nullptr);missing=false;
 DmaChannel detached;sb[0].dma.chan=&detached;std::ostringstream unknown;POD_Save_Sblaster(unknown);assert(unknown.fail());
}
'''
(w/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual SB save/load: both cards/absent combinations, every truncation, all DMA IDs, unavailable native DMA, invalid presence ASan/UBSan PASS; doubles, not sound semantics')
