#!/usr/bin/env python3
"""Actual callback admission predicate, plus all six pre-effect entry boundaries."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'fresh-callback-entry'
work.mkdir(exist_ok=True)
s = (root / 'src/cpu/callback.cpp').read_text()
start = s.index('static void RequireGuestCallbackEntry() {')
end = s.index('\n}\n', start) + 3
predicate = s[start:end]
machine = (root / 'src/dosbox.cpp').read_text()
body = machine.split('void DOSBOX_RunMachine(MachineService::Kind kind){', 1)[1]
start = body.index('    if (!control->opt_restore_state.empty()')
end = body.index('    MachineService::Coordinator::Scope')
dispatch = body[start:end]
assert 'RestoredSession::selected()' in dispatch and 'Kind::RootGuest' in dispatch
# Each helper must reject before its native-suffix scope, flags, stack,
# registers, cycle accounting, events or machine dispatch. This is a bounded
# entry-order assertion, not an integrated constructor/no-replay qualification.
entries = ['void CALLBACK_Idle(void)', 'void CALLBACK_IdleNoInts(void)',
           'void CALLBACK_RunRealFarInt(uint16_t seg,uint16_t off)',
           'void CALLBACK_RunRealFar(uint16_t seg,uint16_t off)',
           'void CALLBACK_RunRealInt_retcsip(uint8_t intnum,Bitu &rcs,Bitu &rip)',
           'static void CALLBACK_RunRealIntOwned(uint8_t intnum,MachineService::Kind kind)']
for entry in entries:
    assert s.split(entry + ' {', 1)[1].lstrip().startswith('RequireGuestCallbackEntry();'), entry
source = r'''
#include <cassert>
#include <string>
#include <stdexcept>
struct Control {std::string opt_restore_state;} storage, *control=&storage;
namespace RestoredSession {bool active=false;bool selected(){return active;}}
void E_Exit(const char *s){throw std::runtime_error(s);}
''' + predicate + '''
namespace MachineService {enum class Kind {RootGuest,FaultGuest,DiskGuest,Unclassified};}
void dispatch(MachineService::Kind kind) {
''' + dispatch + '''
}
''' + r'''
int effects=0;
void entry(){RequireGuestCallbackEntry();++effects;}
int main(){
 entry();assert(effects==1);
 dispatch(MachineService::Kind::DiskGuest);
 control->opt_restore_state="input.sav";
 dispatch(MachineService::Kind::RootGuest);
 for (auto kind : {MachineService::Kind::FaultGuest, MachineService::Kind::DiskGuest,
                   MachineService::Kind::Unclassified}) {
  try {dispatch(kind);assert(false);}catch(const std::runtime_error &){}
 }
 try {entry();assert(false);}catch(const std::runtime_error &e){
  assert(std::string(e.what()).find("guest callback required")!=std::string::npos);
 }
 assert(effects==1 && control->opt_restore_state=="input.sav");
 RestoredSession::active=true;entry();assert(effects==2);
}
'''
p = work / 'entry.cpp'
p.write_text(source)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=undefined', str(p), '-o', str(work / 'entry')], check=True)
subprocess.run([str(work / 'entry')], check=True)
print('Actual callback admission: ordinary/selected allowed; unused fresh target rejects before effects; six entry boundaries checked')
