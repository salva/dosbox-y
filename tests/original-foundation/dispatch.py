#!/usr/bin/env python3
"""Exercise actual shared dispatch bodies with deterministic native doubles."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'dispatch-regression'
work.mkdir(exist_ok=True)
source = (root / 'src/dosbox.cpp').read_text()
bodies = source[source.index('Bits DOSBOX_DispatchDecoder()'):source.index('static Bitu Normal_Loop(void)')]
fixture = r'''
#include <cassert>
#include <stdexcept>
using Bits=int; using Bitu=unsigned;
constexpr unsigned CB_MAX=4;
bool dosbox_allow_nonrecursive_page_fault=false;
unsigned last_callback=9;
bool fail=false;
int decoder() {assert(dosbox_allow_nonrecursive_page_fault);if(fail)throw 1;return 2;}
int (*cpudecoder)()=decoder;
unsigned handler() {assert(!dosbox_allow_nonrecursive_page_fault);assert(last_callback==2);if(fail)throw 2;return 3;}
unsigned (*CallBack_Handlers[CB_MAX])()={nullptr,nullptr,handler,nullptr};
[[noreturn]] void E_Exit(const char*) {throw std::runtime_error("invalid callback");}
'''
fixture += bodies
machine = source[source.index('void DOSBOX_RunMachine(void)'):source.index('uint32_t turbolasttick')]
fixture += '\nBitu loop_return=1; bool loop_throw=false;\nBitu machine_loop(){assert(last_callback==0);if(loop_throw)throw 3;return loop_return;}\nBitu (*loop)()=machine_loop;\n'
fixture += machine
fixture += r'''
int main() {
 DOSBOX_RunMachine();assert(last_callback==9);
 loop_throw=true;
 try {DOSBOX_RunMachine();assert(false);}catch(int n){assert(n==3);}
 assert(last_callback==9);
 for(bool initial:{false,true}) {
  dosbox_allow_nonrecursive_page_fault=initial;
  fail=false;assert(DOSBOX_DispatchDecoder()==2);
  assert(dosbox_allow_nonrecursive_page_fault==initial);
  assert(DOSBOX_DispatchCallback(2)==3);
  assert(last_callback==9 && dosbox_allow_nonrecursive_page_fault==initial);
  fail=true;
  try {DOSBOX_DispatchDecoder();assert(false);}catch(int n){assert(n==1);}
  assert(dosbox_allow_nonrecursive_page_fault==initial);
  try {DOSBOX_DispatchCallback(2);assert(false);}catch(int n){assert(n==2);}
  assert(last_callback==9 && dosbox_allow_nonrecursive_page_fault==initial);
  for(int invalid:{-1,0,4}) {
   try {DOSBOX_DispatchCallback(invalid);assert(false);}catch(const std::runtime_error&){}
   assert(last_callback==9 && dosbox_allow_nonrecursive_page_fault==initial);
  }
 }
}
'''
(work / 'dispatch.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror',str(work / 'dispatch.cpp'),'-o',str(work / 'dispatch')],check=True)
subprocess.run([str(work / 'dispatch')],check=True)
print('shared actual decoder/callback normal-return, throw and invalid-index restoration PASS')
