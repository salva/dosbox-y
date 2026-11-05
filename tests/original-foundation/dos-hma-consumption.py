#!/usr/bin/env python3
"""Consume actual DOS HMA cursor codec, with reduced service availability."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-hma-consumption';w.mkdir(exist_ok=True)
s=(root/'src/dos/dos.cpp').read_text();body=s[s.index('static void DOS_HMAState'):s.index('namespace\n{\n\tclass SerializeDos')]
code=r'''
#include <cstdint>
#include <cassert>
#include <sstream>
uint32_t dos_hma_allocator=0;int dos_initial_hma_free=0xff00;
bool available=true;uint32_t DOS_HMA_LIMIT(){return available?0x10fff0:0;}
'''+body+r'''
int main(){
 for(auto cursor:{0u,0x1000f0u,0x100123u,0x10fff0u}){
  dos_hma_allocator=cursor;std::ostringstream out;DOS_HMAState(&out,nullptr);assert(out.good());assert(dos_hma_allocator==cursor);
  dos_hma_allocator=0;std::istringstream in(out.str());DOS_HMAState(nullptr,&in);assert(in.good());assert(dos_hma_allocator==cursor);
  for(unsigned n=0;n<4;++n){dos_hma_allocator=17;std::istringstream short_in(out.str().substr(0,n));DOS_HMAState(nullptr,&short_in);assert(short_in.fail());assert(dos_hma_allocator==17);}
 }
 for(auto bad:{1u,0x1000efu,0x10fff1u,0xffffffffu}){
  dos_hma_allocator=bad;std::ostringstream out;DOS_HMAState(&out,nullptr);assert(out.fail());assert(out.str().empty());
  std::string bytes((char*)&bad,4);dos_hma_allocator=0;std::istringstream in(bytes);DOS_HMAState(nullptr,&in);assert(in.fail());assert(dos_hma_allocator==0);
 }
 available=false;dos_hma_allocator=0x1000f0;std::ostringstream out;DOS_HMAState(&out,nullptr);assert(out.fail());
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-fsanitize=undefined',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual DOS HMA codec: uninitialized/allocated/exhausted cursor roundtrip, invalid/short/unavailable reject (reduced UBSan)')
