#!/usr/bin/env python3
"""Actual MSCDEX transport load with resource-failure doubles, no CD claim."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mscdex-resource-consumption'
work.mkdir(exist_ok=True)
text=(root/'src/dos/dos_mscdex.cpp').read_text()
body=text[text.index('void POD_Load_DOS_Mscdex'):]
source=r'''
#include <cstdint>
#include <cassert>
#include <sstream>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
bool dos_kernel_disabled=false;
constexpr unsigned MSCDEX_MAX_DRIVES=8;
unsigned GetNumDrives(){return 1;}
struct TMSF {uint8_t min,sec,fr;};
bool stop_ok=true,play_ok=true;
unsigned stop_calls=0,play_calls=0,load_calls=0;
bool StopAudio(unsigned){++stop_calls;return stop_ok;}
struct Mscdex {
 bool PlayAudioMSF(unsigned,uint32_t,uint32_t){++play_calls;return play_ok;}
 void LoadState(std::istream&){++load_calls;}
} instance,*mscdex=&instance;
'''+body+r'''
int main(){
 for(unsigned active=0;active<3;++active){
  std::ostringstream output;
  uint16_t count=1;uint8_t playing=active==1,pause=active==2;
  TMSF pos={0,2,0},start={0,2,0},end={0,4,0};
  output.write((char*)&count,2);output.write((char*)&playing,1);output.write((char*)&pause,1);
  for(auto v:{pos,start,end})output.write((char*)&v,sizeof(v));
  for(unsigned failure=0;failure<3;++failure){
   stop_ok=failure!=1;play_ok=failure!=2;stop_calls=play_calls=load_calls=0;
   std::istringstream input(output.str());POD_Load_DOS_Mscdex(input);
   assert(stop_calls==1);
   const bool rejects=failure==1 || (active && failure==2);
   assert(input.fail()==rejects);assert(load_calls==unsigned(!rejects));
   assert(play_calls==unsigned(active && stop_ok));
  }
 }
}
'''
(work/'test.cpp').write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-fsanitize=undefined',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual MSCDEX load: stop/play/pause resource failures reject before suffix (reduced UBSan)')
