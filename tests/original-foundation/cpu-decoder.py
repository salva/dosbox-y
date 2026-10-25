#!/usr/bin/env python3
"""Actual CPU decoder identity consumption; not pre-effect CPU validation."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'cpu-decoder'
work.mkdir(exist_ok=True)
t=(root/'src/cpu/cpu.cpp').read_text()
a=t.index('uint16_t CPU_FindDecoderType(')
b=t.index('extern void POD_Save_CPU_Flags',a)
mapping=t[a:b]
a=t.index('    // Earlier CPU fields are already live.')
b=t.index('    POD_Load_CPU_Flags(stream);',a)
check=t[a:b]
s='''
#include <cassert>
#include <cstdint>
#include <sstream>
using CPU_Decoder=int();
int CPU_Core_Normal_Run(){return 0;} int CPU_Core_Prefetch_Run(){return 0;}
int CPU_Core_Simple_Run(){return 0;} int CPU_Core_Full_Run(){return 0;}
int CPU_Core_Dyn_X86_Run(){return 0;} int CPU_Core_Dynrec_Run(){return 0;}
int CPU_Core_Normal_Trap_Run(){return 0;} int CPU_Core_Dyn_X86_Trap_Run(){return 0;}
int CPU_Core_Dynrec_Trap_Run(){return 0;} int HLT_Decode(){return 0;}
[[noreturn]] void E_Exit(const char*){throw 7;}
'''+mapping+'void consume(std::istream &stream,uint16_t decoder_idx){\n'+check+'}\n'+'''
int main(){
 for(unsigned i=0;i<65536;++i){
  const bool valid=i==0 || i==1 || i==100 || i==200 ||
   (!C_EMSCRIPTEN && (i==2 || i==3)) ||
   (C_DYNAMIC_X86 && (i==4 || i==101)) || (C_DYNREC && (i==5 || i==102));
  auto *mapped=CPU_IndexDecoderType(uint16_t(i));
  assert(bool(mapped)==valid);
  if(mapped)assert(CPU_FindDecoderType(mapped)==i);
  assert(CPU_FindDecoderType(nullptr)==0xffff);
  std::istringstream stream;bool fatal=false;
  try{consume(stream,uint16_t(i));}catch(int n){assert(n==7);fatal=true;}
  assert(fatal==!valid);
 }
 for(unsigned i:{0u,100u,200u}){
  std::istringstream stream;stream.setstate(std::ios::failbit);bool fatal=false;
  try{consume(stream,uint16_t(i));}catch(int n){fatal=n==7;}assert(fatal);
 }
}
'''
# Production uses defined(C_EMSCRIPTEN), so omit the macro on ordinary hosts.
s=s.replace('!C_EMSCRIPTEN','!EMSCRIPTEN_TEST')
(work/'test.cpp').write_text(s)
for em in (0,1):
 for dyn in (0,1):
  for rec in (0,1):
   args=['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',f'-DEMSCRIPTEN_TEST={em}',f'-DC_DYNAMIC_X86={dyn}',f'-DC_DYNREC={rec}']
   if em: args+=['-DC_EMSCRIPTEN=1']
   subprocess.run(args+[str(work/'test.cpp'),'-o',str(work/'test')],check=True)
   subprocess.run([str(work/'test')],check=True)
print('CPU decoder consumed identity ASan/UBSan PASS (8 build configurations, not whole CPU preparation)')
