#!/usr/bin/env python3
"""Compile actual lazy flag owner with its native headers; no CPU execution."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'flags-observation-regression'
work.mkdir(exist_ok=True)
fixture = r'''
#include <cassert>
#include <cstring>
#include "cpu.h"
#include "lazyflags.h"
#include "logging.h"
void LOG::operator()(char const*, ...) {}
CPU_Regs cpu_regs = {};
Segments Segs = {};
Bitu CPU_ArchitectureType_fixture = 0;
int main(){
 for(unsigned type=0;type<t_LASTFLAG;++type){
  if(type==t_NOTDONE)continue;
  for(unsigned value:{0u,1u,0x7fu,0x80u,0xffffu,0x80000000u,0xffffffffu}){
   lflags={};lflags.type=type;lflags.prev_type=t_ADDb;lflags.oldcf=1;
   lf_var1d=value;lf_var2d=1;lf_resd=value+1;
   reg_flags=0xa55a;
   const auto saved=lflags;const auto flags=reg_flags;
   const auto expected=FillFlags();
   lflags=saved;reg_flags=flags;
   assert(CPU_InspectFlags()==expected);
   assert(!memcmp(&saved,&lflags,sizeof saved) && reg_flags==flags);
   assert(CPU_InspectFlags()==expected);
  }
 }
}
'''
(work / 'flags.cpp').write_text(fixture)
build = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'foundation-build'
subprocess.run(['g++','-std=gnu++14','-DHAVE_CONFIG_H','-ffunction-sections','-fdata-sections','-I',str(build),'-I',str(root/'include'),'-I',str(root/'src/cpu'),str(work/'flags.cpp'),str(root/'src/cpu/flags.cpp'),'-Wl,--gc-sections','-o',str(work/'flags')],check=True)
subprocess.run([str(work/'flags')],check=True)
print('actual lazy flag observation equals materialization and retains owner state PASS')
