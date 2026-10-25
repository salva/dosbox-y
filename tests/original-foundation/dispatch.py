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
#include "machine_service.h"
#include "native_frontier.h"
#include "native_slice_observer.h"
MachineService::Owner DOSBOX_MachineServiceOwner();
using Bits=int; using Bitu=unsigned;
constexpr unsigned CB_MAX=4;
bool dosbox_allow_nonrecursive_page_fault=false;
unsigned last_callback=9;
bool fail=false;
int decoder() {assert(dosbox_allow_nonrecursive_page_fault);NativeFrontier::frontiers().reach(NativeFrontier::Kind::PartialParent,1);if(fail)throw 1;return 2;}
int (*cpudecoder)()=decoder;
unsigned handler() {assert(!dosbox_allow_nonrecursive_page_fault);assert(last_callback==2);NativeFrontier::frontiers().reach(NativeFrontier::Kind::PartialParent,1);if(fail)throw 2;return 3;}
unsigned (*CallBack_Handlers[CB_MAX])()={nullptr,nullptr,handler,nullptr};
[[noreturn]] void E_Exit(const char*) {throw std::runtime_error("invalid callback");}
'''
fixture += bodies
machine = source[source.index('static MachineService::Coordinator machine_service;'):source.index('uint32_t turbolasttick')]
fixture += '\nBitu loop_return=1; bool loop_throw=false;\nBitu machine_loop(){assert(last_callback==0);if(loop_throw)throw 3;if(!loop_return){loop_return=1;return 0;}return loop_return;}\nBitu (*loop)()=machine_loop;\n'
fixture += machine
fixture += r'''
unsigned services=0;
void service(void*) {
 assert(DOSBOX_MachineServiceOwner().kind==MachineService::Kind::RootGuest);
 assert(DOSBOX_MachineServiceAdmission()==MachineService::Denial::Active);
 ++services;
}
unsigned frontier_calls=0;
void frontier(void*,const NativeFrontier::Descriptor &d) {
 assert(NativeFrontier::frontiers().ownsHold(d));
 if(d.kind==NativeFrontier::Kind::PartialParent){assert(dosbox_allow_nonrecursive_page_fault);assert(d.decoder_entered);}
 if(d.callback_entered)assert(!d.decoder_entered);
 if(d.kind==NativeFrontier::Kind::NativeReturn && !d.callback_entered)assert(!d.decoder_entered);
 ++frontier_calls;
}
unsigned slice_before=0,slice_after=0;
bool incoming_allow=false;
void slice(void*,bool before) noexcept {
 if(before){assert(dosbox_allow_nonrecursive_page_fault);++slice_before;}
 else{assert(dosbox_allow_nonrecursive_page_fault==incoming_allow);++slice_after;}
}
int main() {
 assert(NativeExecution::slice_observer().bind(slice,nullptr));
 assert(NativeFrontier::frontiers().bind(frontier,nullptr));
 DOSBOX_SetMachineService(service,nullptr);
 DOSBOX_RequestMachineService();
 DOSBOX_RunMachine();assert(services==0); // Nonzero result never services.
 {
  MachineService::Coordinator::Scope root(machine_service,MachineService::Kind::RootGuest);
  assert(DOSBOX_EnqueueMachineService(root.token()));
  assert(machine_service.service(machine_service_slice,machine_service_context));
  assert(services==1 && !machine_service.pending());
  {
   MachineService::Coordinator::Scope nested(machine_service,MachineService::Kind::CallbackGuest);
   assert(!DOSBOX_EnqueueMachineService(nested.token()));
   DOSBOX_RequestMachineService();
   assert(!machine_service.service(machine_service_slice,machine_service_context));
  }
  assert(machine_service.service(machine_service_slice,machine_service_context));
  assert(services==2);
 }
 assert(DOSBOX_MachineServiceOwner().token==0);
 DOSBOX_RequestMachineService();loop_return=0;
 DOSBOX_RunMachine(MachineService::Kind::RootGuest);
 assert(services==3 && !machine_service.pending());
 DOSBOX_SetMachineService(nullptr,nullptr);
 DOSBOX_RunMachine();assert(last_callback==9);
 loop_throw=true;
 try {DOSBOX_RunMachine();assert(false);}catch(int n){assert(n==3);}
 assert(last_callback==9);
 for(bool initial:{false,true}) {
  incoming_allow=initial;
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
  NativeFrontier::frontiers().reach(NativeFrontier::Kind::NativeReturn,1);
  assert(!NativeFrontier::frontiers().holding());
  for(int invalid:{-1,0,4}) {
   try {DOSBOX_DispatchCallback(invalid);assert(false);}catch(const std::runtime_error&){}
   assert(last_callback==9 && dosbox_allow_nonrecursive_page_fault==initial);
  }
 }
 assert(slice_before==4 && slice_after==4);
 assert(NativeExecution::slice_observer().unbind(slice,nullptr));
}
'''
(work / 'dispatch.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-I',str(root / 'include'),str(work / 'dispatch.cpp'),'-o',str(work / 'dispatch')],check=True)
subprocess.run([str(work / 'dispatch')],check=True)
print('shared actual decoder/callback normal-return, throw and invalid-index restoration PASS')
