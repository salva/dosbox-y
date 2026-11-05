#!/usr/bin/env python3
"""Actual live-load entry chain: diagnostic refusal without file/guest effects.
Reduced SaveState/log doubles; integrated debugger/slot/checkpoint coverage lives
in stopped-save.py and tests/machine-service checkpoint consumers.
"""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'live-load-refusal'
w.mkdir(exist_ok=True)
s=(root/'src/misc/savestates.cpp').read_text()
a=s.index('void SaveState::load(size_t slot) const')
b=s.index('void SaveState::restoreFreshStartup',a)
wrappers=s[a:b]
a=s.index('bool SaveState::loadFrom(size_t slot,')
b=s.index('bool SaveState::isEmpty',a)
code=r'''
#include <cassert>
#include <string>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
unsigned logs=0;std::string message;
void LOG_MSG(const char *format,...){char text[1024];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);message=text;++logs;}
struct SaveState{
 void load(size_t) const;
 bool loadFile(const std::string&) const;
 bool loadFrom(size_t,const std::string*) const;
};
'''+wrappers+s[a:b]+r'''
int main(){SaveState state;
 for(size_t slot:{size_t(0),size_t(99),size_t(-1)}){
  auto before=logs;state.load(slot);assert(logs==before+1);
  assert(message.find("fresh launch required")!=std::string::npos);
 }
 for(const std::string &path:{std::string(),std::string("missing.sav"),std::string("/dev/zero")}){
  auto before=logs;assert(!state.loadFile(path));assert(logs==before+1);
  assert(message.find("fresh launch required")!=std::string::npos);
 }
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
debug=(root/'src/debug/debug.cpp').read_text()
start=debug.index('bool DEBUG_NativeStateSlot(')
end=debug.index('bool DEBUG_NativeStateFile(',start)
slot_adapter=debug[start:end]
# Compile the real early-return branches with deliberately poisonous authority
# doubles. A reordered attachment/admission/lease would fail this consumer.
file_start=end
file_end=debug.index('\n}',file_start)+2
file_adapter=debug[file_start:file_end]
adapters=r'''
#include <cassert>
#include <string>
#include <cstddef>
#include <cstdint>
namespace StateArchive {struct Effects {bool touched=false;};struct Budget {};}
namespace NativeDebug {struct Attachment {};struct Context {uint64_t invocation=1,checkpoint=1;};enum class Frontend {Interactive};}
namespace NativeControl {
 enum class Admission {ok};struct Lease {};
 struct Owner {Admission acquire(NativeDebug::Context,int,int,uint64_t,bool,int,uint64_t){assert(false);return Admission::ok;}Lease lease(){assert(false);return {};}void release(Lease){assert(false);}};
 Owner&owner(){assert(false);static Owner o;return o;}
}
namespace RestoredSession {struct Invocation {bool owns(uint64_t){assert(false);return false;}};Invocation&invocation(){assert(false);static Invocation i;return i;}}
NativeDebug::Attachment DEBUG_AttachNativeExecution(NativeDebug::Frontend,int){assert(false);return {};}
NativeDebug::Context DEBUG_NativeExecutionContext(){assert(false);return {};}
bool DEBUG_AdmitNativeState(NativeDebug::Attachment,NativeDebug::Context){assert(false);return false;}
int DOSBOX_MachineServiceOwner(){assert(false);return 0;}
int DOSBOX_MachineServiceRelation(uint64_t){assert(false);return 0;}
int DOSBOX_MachineContextAdmission(){assert(false);return 0;}
unsigned loads=0;
struct SaveState {
 static SaveState&instance(){static SaveState s;return s;}
 bool loadFrom(size_t,const std::string*)const{++loads;return false;}
 bool loadFile(const std::string&)const{++loads;return false;}
 bool saveTo(size_t,const std::string*,StateArchive::Effects*,const StateArchive::Budget*,const std::string & = ""){assert(false);return false;}
 bool saveFile(const std::string&){assert(false);return false;}
 bool saveFile(const std::string&,StateArchive::Effects&,const StateArchive::Budget*){assert(false);return false;}
};
'''+slot_adapter+file_adapter+r'''
int main(){
 assert(!DEBUG_NativeStateSlot(size_t(-1),true,"ignored"));
 StateArchive::Effects effects;effects.touched=true;
 assert(!DEBUG_NativeStateFile({}, {}, "/dev/zero",true,&effects,nullptr));
 assert(!effects.touched && loads==2);
}
'''
(w/'adapters.cpp').write_text(adapters)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-fsanitize=undefined',str(w/'adapters.cpp'),'-o',str(w/'adapters')],check=True)
subprocess.run([str(w/'adapters')],check=True)
print('Actual debugger slot/file load adapters refuse before attachment/admission/lease; supplied effects cleared (poison authority doubles)')
print('Actual load/loadFile/loadFrom unconditional fresh-launch refusal, no file/guest dependency (reduced owner/log doubles)')
