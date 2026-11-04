#!/usr/bin/env python3
"""Actual debugger slot/file load refusal before attachment/lease/native reads.
Save-side authority is mocked; this is adapter ordering, not state qualification.
"""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'debug-state-refusal'
w.mkdir(exist_ok=True)
text = (root / 'src/debug/debug.cpp').read_text()
start = text.index('bool DEBUG_NativeStateSlot(')
end = text.index('const NativeDebug::Result *DEBUG_NativeExecutionResult(', start)
source = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
static unsigned reads=0, releases=0, refusals=0;
namespace NativeDebug {
struct Attachment {};
struct Context {unsigned invocation=7,checkpoint=1;};
enum class Frontend {Interactive};
}
namespace StateArchive {struct Effects {bool published=false;};struct Budget {};}
namespace MachineService {enum class Denial {None};}
static NativeDebug::Attachment DEBUG_AttachNativeExecution(NativeDebug::Frontend,unsigned) {++reads;return {};}
static NativeDebug::Context DEBUG_NativeExecutionContext() {++reads;return {};}
static bool DEBUG_AdmitNativeState(NativeDebug::Attachment,NativeDebug::Context) {++reads;return true;}
static unsigned DOSBOX_MachineServiceOwner(){++reads;return 0;}
static unsigned DOSBOX_MachineServiceRelation(unsigned){++reads;return 0;}
static MachineService::Denial DOSBOX_MachineContextAdmission(){++reads;return MachineService::Denial::None;}
namespace RestoredSession {
struct Invocation {bool owns(unsigned){++reads;return true;}};
static Invocation &invocation(){static Invocation i;++reads;return i;}
}
namespace NativeControl {
enum class Admission {ok};
struct Lease {};
struct Owner {
 Admission acquire(std::pair<unsigned,unsigned>,unsigned,unsigned,unsigned,bool,MachineService::Denial,unsigned) {++reads;return Admission::ok;}
 Lease lease(){++reads;return {};}
 void release(Lease){++releases;}
};
static Owner &owner(){static Owner o;++reads;return o;}
}
struct SaveState {
 static SaveState &instance(){static SaveState s;return s;}
 bool loadFrom(size_t,const std::string*){++refusals;return false;}
 bool loadFile(const std::string&){++refusals;return false;}
 bool saveTo(size_t,const std::string*,StateArchive::Effects*,const StateArchive::Budget*,const std::string&){return true;}
 bool saveFile(const std::string&,StateArchive::Effects&,const StateArchive::Budget*){return true;}
 bool saveFile(const std::string&){return true;}
};
'''
source += text[start:end]
source += r'''
int main() {
 assert(!DEBUG_NativeStateSlot(0,true,""));
 assert(!DEBUG_NativeStateSlot(9999,true,""));
 StateArchive::Effects effects{true};
 for(const auto &path : {std::string(),std::string("missing"),std::string("a\0b",3)})
  assert(!DEBUG_NativeStateFile({}, {}, path,true,&effects,nullptr));
 assert(!effects.published && refusals==5 && !reads && !releases);
 assert(DEBUG_NativeStateSlot(0,false,""));
 assert(reads && releases==1);
 const auto before=reads;
 assert(DEBUG_NativeStateFile({}, {}, "output",false,&effects,nullptr));
 assert(reads>before && releases==2);
}
'''
(w/'refusal.cpp').write_text(source)
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                str(w/'refusal.cpp'),'-o',str(w/'refusal')],check=True)
subprocess.run([str(w/'refusal')],check=True)
print('actual debugger slot/file early load refusal, no attachment/native reads/releases; mocked save authority PASS')
