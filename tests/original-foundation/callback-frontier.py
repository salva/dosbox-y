#!/usr/bin/env python3
"""Actual callback wrapper setup/restoration and native frontier restrictions."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'callback-frontier-regression'
work.mkdir(exist_ok=True)
source = (root / 'src/cpu/callback.cpp').read_text()
body = source[source.index('static void CALLBACK_RunRealIntOwned'):source.index('\nnamespace {', source.index('static void CALLBACK_RunRealIntOwned'))]
fixture = r'''
#include <cassert>
#include <stdexcept>
#include "machine_service.h"
#include "native_frontier.h"
using Bitu=unsigned;
unsigned reg_eip=0x123456,reg_ax=0x4b00;
unsigned short cs=0x1234;
enum {CB_SOFFSET=0x100,CB_MAX=8,CB_SEG=0xf000};
unsigned short SegValue(int){return cs;}
void SegSet16(int,unsigned short v){cs=v;}
MachineService::Owner DOSBOX_MachineServiceOwner(){return {MachineService::Kind::RootGuest,1,1};}
bool fail=false;unsigned observations=0;
MachineService::Kind expected;
void DOSBOX_RunMachine(MachineService::Kind kind){
 assert(kind==expected && cs==CB_SEG && reg_eip==CB_SOFFSET+CB_MAX*6+(kind==MachineService::Kind::ShellExecGuest?0x21:0x10)*6);
 NativeFrontier::frontiers().reach(NativeFrontier::Kind::NestedEntry,2);
 if(fail)throw std::runtime_error("native failure");
}
void hook(void*,const NativeFrontier::Descriptor &d){
 assert(NativeFrontier::frontiers().ownsHold(d));
 assert(d.callback_entered==(expected!=MachineService::Kind::ShellExecGuest));
 if(d.kind==NativeFrontier::Kind::NativeReturn)assert(cs==0x1234 && reg_eip==0x123456);
 ++observations;
}
'''
# The real body uses CB_SIZE for its private INT trampoline layout.
fixture = fixture.replace('CB_MAX=8,', 'CB_MAX=8,CB_SIZE=6,')
fixture += body
fixture += r'''
int main(){
 assert(NativeFrontier::frontiers().bind(hook,nullptr));
 for(auto kind:{MachineService::Kind::CallbackGuest,MachineService::Kind::ShellExecGuest}){
  expected=kind;cs=0x1234;reg_eip=0x123456;fail=false;
  if(kind==MachineService::Kind::CallbackGuest)CALLBACK_RunRealInt(0x10);else CALLBACK_RunShellExec();
  assert(cs==0x1234 && reg_eip==0x123456 && observations);
  fail=true;
  try{CALLBACK_RunRealIntOwned(0x10,kind);assert(false);}catch(const std::runtime_error&){}
  // Upstream does not restore guest registers on an exceptional exit; only
  // scope identity expires. This test does not invent rollback/disposal.
  assert(!NativeFrontier::frontiers().holding());
 }
 assert(NativeFrontier::frontiers().unbind(hook,nullptr));
}
'''
# Exceptional call must use the expected trampoline too.
fixture = fixture.replace('CALLBACK_RunRealIntOwned(0x10,kind);assert(false);', 'CALLBACK_RunRealIntOwned(kind==MachineService::Kind::ShellExecGuest?0x21:0x10,kind);assert(false);')
(work / 'callback.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-I',str(root / 'include'),str(work / 'callback.cpp'),'-o',str(work / 'callback')],check=True)
subprocess.run([str(work / 'callback')],check=True)
print('actual INT callback/ShellExec setup, restoration, frontier restriction and exceptional scope expiry PASS')
