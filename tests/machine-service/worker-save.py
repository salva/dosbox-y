#!/usr/bin/env python3
"""Actual worker save projection; mocked native operation/authority, not integration."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
area=Path(tempfile.mkdtemp(prefix='worker-save-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
s=(root/'src/debug/debug_lua.cpp').read_text()
s=s[s.index('void Debugger_LocalDriverSave('):s.index('const char *Debugger_LocalDriverLoad(')]
preamble=r'''
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "lua_driver_handoff.h"
#include "state_archive_effects.h"
namespace SaveState {using Error=std::string;}
static bool admitted=true,success=true,fail_cleanup=false,publish=false,lose_authority=false;
static int failure=0;
static bool DEBUG_AdmitNativeState(NativeDebug::Attachment,NativeDebug::Context) {return admitted;}
static bool DEBUG_NativeStateFile(NativeDebug::Attachment,NativeDebug::Context,const std::string &,bool,StateArchive::Effects *e,const StateArchive::Budget *budget) {
 assert(budget);
 if(budget->expired()){e->timed_out=true;return false;}
 if(lose_authority)admitted=false;
 e->published=publish;e->cleanup_attempted=fail_cleanup;e->cleanup_failed=fail_cleanup;
 if(failure==1)throw std::runtime_error("serializer");
 if(failure==2)throw SaveState::Error("serializer");
 if(failure==3)throw 7;
 return success;
}
'''
main=r'''
int main() {
 LuaDriver::SaveEffects e;
 Debugger_LocalDriverSave({}, {}, "out",0,e);
 assert(e.valid && !e.completed && e.retired && e.usable && e.code=="timeout");
 Debugger_LocalDriverSave({}, {}, "out",1,e);
 assert(e.valid && e.completed && e.retired && e.usable && e.code=="saved");
 lose_authority=true;publish=true;
 Debugger_LocalDriverSave({}, {}, "out",1,e);
 assert(e.completed && e.published && !e.usable);
 lose_authority=false;admitted=false;success=false;
 Debugger_LocalDriverSave({}, {}, "out",1,e);
 assert(e.valid && !e.completed && e.retired && !e.usable);
 admitted=true;publish=true;
 for(int f=1;f<=2;++f) {
  failure=f;Debugger_LocalDriverSave({}, {}, "out",1,e);
  assert(!e.completed && e.published && e.usable && e.code=="save_refused_or_failed");
 }
 failure=0;fail_cleanup=true;
 Debugger_LocalDriverSave({}, {}, "out",1,e);
 assert(e.cleanup_attempted && e.cleanup_failed && !e.usable);
 failure=3;bool fatal=false;
 try {Debugger_LocalDriverSave({}, {}, "out",1,e);}catch(int n){fatal=n==7;}
 assert(fatal);
}
'''
(area/'test.cpp').write_text(preamble+s+main)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-I'+str(root/'include'),str(area/'test.cpp'),'-o',str(area/'test')],check=True)
subprocess.run([str(area/'test')],check=True)
print('actual worker save effects/current-authority projection PASS (native mocked)')
