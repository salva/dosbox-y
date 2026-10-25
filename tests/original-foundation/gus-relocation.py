#!/usr/bin/env python3
"""Actual GUS relocation consumption; not complete sound-owner preflight."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root / 'src/hardware/gus.cpp').read_text()
a = s.index('\t// Relocation is consumed only')
b = s.index('\n\t//*******************************************', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'gus-relocation'
w.mkdir(exist_ok=True)
code = r'''
#include <sstream>
#include <cstdint>
#include <cassert>
struct Channel{} channels[32], sentinel;
Channel* guschan[32];
Channel* curchan=&sentinel;
void relocate(std::istream& stream, uint8_t curchan_idx) {
''' + s[a:b] + r'''
}
int main() {
 for(unsigned i=0;i<32;++i)guschan[i]=&channels[i];
 for(unsigned i=0;i<256;++i){
  std::istringstream stream; curchan=&sentinel;
  relocate(stream, static_cast<uint8_t>(i));
  if(i<32){assert(stream);assert(curchan==guschan[i]);}
  else if(i==255){assert(stream);assert(!curchan);}
  else {assert(stream.fail());assert(curchan==&sentinel);}
 }
 std::istringstream truncated;truncated.setstate(std::istream::failbit);
 curchan=&sentinel;relocate(truncated,0);assert(curchan==&sentinel);
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual GUS relocation all byte indices and failed-stream preservation ASan/UBSan PASS; owner doubles')
