#!/usr/bin/env python3
"""Actual GUS descriptor fields/consumption with retained native DRAM."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/hardware/gus.cpp').read_text()
a=s.index('struct GFGus {');b=s.index('\n\tvoid updateMasterVolume',a)
fields=s[a:b]+'\n};\n'
a=s.index('\t// Archived host addresses do not own native DRAM.')
b=s.index('\n\tfor( int lcv=0; lcv<32;',a)
body=s[a:b]
a=s.index('\t// DRAM is guest sound state,')
b=s.index('\n\n',a)
save=s[a:b]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'gus-dram';w.mkdir(exist_ok=True)
code=r'''
#include <cstdint>
#include <sstream>
#include <cassert>
#include <cstring>
using Bitu=uintptr_t;
'''+fields+r'''
GFGus myGUS={};
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
void save_content(std::ostream& stream){
'''+save+r'''
}
void load(std::istream& stream){
'''+body+r'''
}
std::string capture(GFGus value){std::string bytes(reinterpret_cast<const char*>(&value),sizeof(value));bytes.append(value.memsize,'\x55');return bytes;}
int main(){
 uint8_t native[64]={};GFGus live={};live.memsize=32;live.gDramAddrMask=31;live.gDramVoiceMask=31;live.GUSRam=native;live.midiData=3;
 auto incoming=live;incoming.GUSRam=nullptr;incoming.midiData=17;
 for(size_t n=0;n<capture(incoming).size();++n){myGUS=live;std::istringstream in(capture(incoming).substr(0,n));load(in);assert(in.fail());assert(myGUS.GUSRam==native && myGUS.midiData==3);}
 for(unsigned mode=0;mode<3;++mode){auto bad=incoming;if(mode==0)bad.memsize++;if(mode==1)bad.gDramAddrMask++;if(mode==2)bad.gDramVoiceMask++;myGUS=live;std::istringstream in(capture(bad));load(in);assert(in.fail());assert(myGUS.GUSRam==native && myGUS.midiData==3);}
 myGUS=live;std::istringstream in(capture(incoming));load(in);assert(in);assert(myGUS.GUSRam==native && myGUS.midiData==17);
 assert(native[0]==0x55 && native[31]==0x55 && native[32]==0);
 myGUS.GUSRam[31]=9;assert(native[31]==9);
 myGUS=live;std::ostringstream saved;save_content(saved);assert(saved && saved.str()==std::string(reinterpret_cast<const char*>(native),32));
 myGUS.GUSRam=nullptr;std::ostringstream missing;save_content(missing);assert(missing.fail());
 myGUS.memsize=0;std::ostringstream empty;save_content(empty);assert(empty && empty.str().empty());
 myGUS=live;myGUS.GUSRam=nullptr;std::istringstream absent(capture(incoming));load(absent);assert(absent.fail());
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual GUS descriptor retains native DRAM, rejects extent mismatch/truncation ASan/UBSan PASS; not full owner validation')
