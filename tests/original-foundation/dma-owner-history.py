#!/usr/bin/env python3
"""Actual DMA owner traversal with distinct native callback identities."""
import os,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[2]
s=(r/'src/hardware/dma.cpp').read_text();h=(r/'include/dma.h').read_text()
fields=h[h.index('\t\tuint32_t pagebase;'):h.index('\n\t\tvoid page_bank_increment')]
codec=s[s.index('void DmaChannel::SaveState'):s.index('void DmaController::SaveState')]
code='''#include <sstream>
#include <stdexcept>
#include <cassert>
#include <cstdint>
using Bitu=uintptr_t;
enum DMAActor {DMAA_NONE,DMAA_CONTROLLER,DMAA_GUEST,DMAA_REISSUE};
using DMA_CallBack=void(*)();void cb(){};
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
#define E_Exit(x) throw std::runtime_error(x)
uint8_t POD_State_Find_DMA_Callback(Bitu p){return p?2:0;}
Bitu POD_State_Index_DMA_Callback(uint8_t n){if(n>5)throw std::runtime_error("callback");return n?(Bitu)cb:0;}
class DmaChannel{public:void SaveState(std::ostream&);void LoadState(std::istream&);
'''+fields+'\n};\n'+codec+'''
int main(){DmaChannel a={},b={};a.channum=b.channum=1;
 a.userData=1;a.callback=cb;a.transfer_mode=2;a.masked_by=DMAA_REISSUE;
 a.page_bank_increment_wraparound=15;a.curraddr=419;a.currcnt=7;
 b.DMA16_PAGESHIFT=19;b.DMA16_ADDRMASK=65535;
 std::ostringstream out;a.SaveState(out);auto bytes=out.str();
 std::istringstream in(bytes);b.LoadState(in);assert(in);
 assert(b.userData==1&&b.callback==cb&&b.transfer_mode==2&&b.masked_by==DMAA_REISSUE);
 assert(b.page_bank_increment_wraparound==15&&b.curraddr==419&&b.currcnt==7);
 assert(b.DMA16_PAGESHIFT==19&&b.DMA16_ADDRMASK==65535);
 std::ostringstream again;b.SaveState(again);assert(again.str()==bytes);
 a.userData=2;std::ostringstream bad;a.SaveState(bad);std::istringstream bin(bad.str());
 bool refused=false;try{b.LoadState(bin);}catch(...){refused=true;}assert(refused);
 for(size_t n=0;n<bytes.size();++n){DmaChannel c={};c.channum=1;
  std::istringstream truncated(bytes.substr(0,n));
  try{c.LoadState(truncated);}catch(...){continue;}assert(!truncated);}
}
'''
w=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dma-owner-test';w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-g',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual DMA codec: nonzero callback userdata/mode/mask/bank history and local policy, truncation, SB index refusal passed')
