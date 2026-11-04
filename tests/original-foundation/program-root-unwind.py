#!/usr/bin/env python3
"""Actual PROGRAMS_Handler terminal propagation; reduced polymorphic owner.

Asserts the deliberate no-destructor rule for RootExit versus ordinary errors
and returns. This is not an integrated DOS_Shell root-exit consumer.
"""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'program-root-unwind'
work.mkdir(exist_ok=True)
source = (root / 'src/misc/programs.cpp').read_text()
handler = source[source.index('static Bitu PROGRAMS_Handler(void) {'):
                 source.index('/* Main functions used in all program */')]
fixture = r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include "restored_session.h"
using Bitu=unsigned;using PhysPt=unsigned;using HostPt=uint8_t*;
constexpr Bitu CBRET_NONE=0;
uint8_t exe_block[3]={};
struct Dos {unsigned psp(){return 0x1234;}} dos;
PhysPt PhysMake(unsigned seg,unsigned off){return seg*16+off;}
uint8_t mem_readb(PhysPt){return 0;}
[[noreturn]] void E_Exit(const char*){throw std::runtime_error("bad program");}
unsigned runs=0,destructors=0;
int mode=0;
struct Program {virtual ~Program(){++destructors;}virtual void Run()=0;};
struct ShellLike:Program {
 void Run() override {
  ++runs;
  if(mode==1)throw RestoredSession::RootExit{{99,0x1234,42,true,true}};
  if(mode==2)throw 7;
 }
} *created=nullptr;
using PROGRAMS_Main=void(Program**);
void construct(Program **p){created=new ShellLike;*p=created;}
struct InternalProgramEntry {PROGRAMS_Main *main;};
InternalProgramEntry entry={construct};
std::vector<InternalProgramEntry*> internal_progs={&entry};
'''+handler+r'''
int main(){
 assert(PROGRAMS_Handler()==CBRET_NONE);
 assert(runs==1 && destructors==1);
 mode=2;
 try{PROGRAMS_Handler();assert(false);}catch(int n){assert(n==7);}
 assert(runs==2 && destructors==2);
 mode=1;
 try{PROGRAMS_Handler();assert(false);}
 catch(const RestoredSession::RootExit &exit){
  assert(exit.facts.invocation==99 && exit.facts.root==0x1234);
  assert(exit.facts.code==42 && exit.facts.resident && exit.facts.self_parent);
 }
 assert(runs==3 && destructors==2); // No guest-mutating shell destruction.
 // Production uses process disposal. The isolated test owns a host-only double
 // and can delete it after proving no destructor ran during actual propagation.
 delete created;assert(destructors==3);
 entry.main=nullptr;assert(PROGRAMS_Handler()==CBRET_NONE);
 assert(runs==3 && destructors==3);
}
'''
(work/'test.cpp').write_text(fixture)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I'+str(root/'include'),
                str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual PROGRAMS_Handler: polymorphic RootExit preserves facts, skips deletion/return; ordinary throw/return delete once (ASan/UBSan)')
