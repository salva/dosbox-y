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
print('Actual load/loadFile/loadFrom unconditional fresh-launch refusal, no file/guest dependency (reduced owner/log doubles)')
