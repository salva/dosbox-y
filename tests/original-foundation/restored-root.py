#!/usr/bin/env python3
"""Always-built root selection/terminal facts; not startup integration proof."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'restored-root';w.mkdir(exist_ok=True)
entry=(root/'src/dos/dos_execute.cpp').read_text().split('void DOS_Terminate(uint16_t pspseg,bool tsr,uint8_t exitcode) {',1)[1].split('dos.return_code=exitcode;',1)[0]
s=r'''
#include "restored_session.h"
#include <cassert>
#include <initializer_list>
using namespace RestoredSession;
RestoredSession::Invocation &RestoredSession::invocation(){static Invocation i;return i;}
uint16_t parent=0x1111;
struct DOS_PSP {DOS_PSP(uint16_t){}uint16_t GetParent(){return parent;}};
unsigned suffix=0;
void actual_entry(uint16_t pspseg,bool tsr,uint8_t exitcode){
'''+entry+r'''
++suffix;
}
int main(){
 auto &owned=invocation();actual_entry(0x1234,false,0);assert(suffix==1);
 assert(owned.select(99,0x1234));
 actual_entry(0x2222,false,0);assert(suffix==2);
 parent=0x1234;
 try {actual_entry(0x1234,true,42);assert(false);}catch(const RootExit &e){
  assert(suffix==2 && e.facts.self_parent && e.facts.resident && e.facts.code==42);
 }
 for(bool tsr:{false,true})for(bool self:{false,true}){
  Invocation i;assert(!i.select(0,10));assert(!i.select(1,0));
  assert(!i.owns(0) && !i.owns(42));
  assert(i.select(42,0x1234));assert(!i.select(43,0x5678));
  assert(i.owns(42) && !i.owns(0) && !i.owns(43));
  i.terminate(0x5678,false,9,false);assert(!i.terminal());
  try{i.terminate(0x1234,tsr,7,self);assert(false);}catch(const RootExit &e){
   assert(e.facts.invocation==42 && e.facts.root==0x1234 && e.facts.code==7);
   assert(e.facts.resident==tsr && e.facts.self_parent==self);
  }
  assert(i.terminal());assert(!i.select(43,0x5678));
  try{i.terminate(0x1234,!tsr,8,!self);assert(false);}catch(const RootExit &e){
   assert(e.facts.code==7 && e.facts.resident==tsr && e.facts.self_parent==self);
  }
 }
 Invocation normal;normal.terminate(0x1234,false,0,false);assert(!normal.terminal());
}
'''
p=w/'root.cpp';p.write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'include'),str(p),'-o',str(w/'root')],check=True)
subprocess.run([str(w/'root')],check=True)
print('immutable root/normal child/normal TSR self-parent terminal latch passed; isolated unit, not startup integration proof')
