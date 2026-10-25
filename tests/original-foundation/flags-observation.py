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
#include <sstream>
#include "cpu.h"
#include "lazyflags.h"
#include "logging.h"
void LOG::operator()(char const*, ...) {}
CPU_Regs cpu_regs = {};
Segments Segs = {};
Bitu CPU_ArchitectureType_fixture = 0;
void POD_Save_CPU_Flags(std::ostream&);
void POD_Load_CPU_Flags(std::istream&);
int main(){
 lflags={};lflags.type=t_ADCd;lflags.oldcf=1;lf_var1d=42;
 std::ostringstream out;POD_Save_CPU_Flags(out);const auto bytes=out.str();
 for(size_t n=0;n<bytes.size();++n){
  lflags={};lflags.type=t_SUBw;lf_var1d=99;const auto before=lflags;
  std::istringstream in(bytes.substr(0,n));POD_Load_CPU_Flags(in);
  assert(in.fail() && !memcmp(&before,&lflags,sizeof before));
 }
 for(unsigned field=0;field<2;++field){
  LazyFlags invalid={};invalid.type=field?t_ADDb:t_LASTFLAG;invalid.oldcf=field?2:0;
  std::string raw(reinterpret_cast<const char*>(&invalid),sizeof invalid);
  const auto before=lflags;std::istringstream in(raw);POD_Load_CPU_Flags(in);
  assert(in.fail() && !memcmp(&before,&lflags,sizeof before));
 }
 std::istringstream complete(bytes);POD_Load_CPU_Flags(complete);
 assert(complete && lflags.type==t_ADCd && lflags.oldcf==1 && lf_var1d==42);
 for(unsigned type=0;type<t_LASTFLAG;++type){
  if(type==t_NOTDONE)continue;
  for(unsigned value:{0u,1u,0x7fu,0x80u,0xffffu,0x80000000u,0xffffffffu}){
   lflags={};lflags.type=type;lflags.prev_type=t_ADDb;lflags.oldcf=1;
   lf_var1d=value;lf_var2d=1;lf_resd=value+1;
   reg_flags=0xa55a;
   std::ostringstream encoded;POD_Save_CPU_Flags(encoded);
   const auto original=lflags;lflags={};
   std::istringstream restored(encoded.str());POD_Load_CPU_Flags(restored);
   assert(restored && !memcmp(&original,&lflags,sizeof original));
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
subprocess.run(['g++','-std=gnu++14','-DHAVE_CONFIG_H','-fsanitize=address,undefined','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections','-I',str(build),'-I',str(root/'include'),'-I',str(root/'src/cpu'),str(work/'flags.cpp'),str(root/'src/cpu/flags.cpp'),'-Wl,--gc-sections','-o',str(work/'flags')],check=True)
subprocess.run([str(work/'flags')],check=True)
print('actual lazy flag observation equals materialization and retains owner state PASS')
