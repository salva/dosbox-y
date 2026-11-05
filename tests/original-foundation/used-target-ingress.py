#!/usr/bin/env python3
"""Execute original refused used-target ingress bodies, debug and native.
Reduced host logging only; source-extracted methods, not a second validator.
"""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'used-target-ingress'
work.mkdir(exist_ok=True)
source=(root/'src/misc/savestates.cpp').read_text()
def body(start,end):
 return source[source.index(start):source.index(end,source.index(start))]
code=r'''
#include <string>
#include <map>
#include <cassert>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
unsigned logs=0;
void LOG_MSG(const char *format,...) {
 char text[512];va_list args;va_start(args,format);
 vsnprintf(text,sizeof(text),format,args);va_end(args);
 assert(std::string(text).find("fresh launch required")!=std::string::npos);
 ++logs;
}
struct SaveState {
 using Checkpoint=std::map<std::string,std::string>;
 void load(size_t) const;
 bool loadFile(const std::string&) const;
 bool loadFrom(size_t,const std::string*) const;
 bool checkpointLoad(const Checkpoint&);
};
'''+body('void SaveState::load(size_t slot)', 'void SaveState::restoreFreshStartup')+body('bool SaveState::loadFrom(size_t slot','bool SaveState::isEmpty')+body('bool SaveState::checkpointLoad(', 'bool SaveState::ComponentOrder::operator()')+r'''
int main() {
 SaveState owner;
 for (size_t slot:{size_t(0),size_t(99),size_t(-1)}) {
  assert(!owner.loadFrom(slot,nullptr));owner.load(slot);
 }
 for (const std::string path:{"", "/nonexistent/archive", "untrusted.sav"})
  assert(!owner.loadFile(path));
 SaveState::Checkpoint parts{{"CPU","malformed"}};
 const auto before=parts;
 assert(!owner.checkpointLoad(parts) && parts==before);
 assert(logs==10);
}
'''
(work/'test.cpp').write_text(code)
for debug in (0,1):
 exe=work/('test-'+str(debug))
 subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',f'-DC_DEBUG={debug}',str(work/'test.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Original load/loadFrom/loadFile/checkpointLoad refuse before source I/O or component consumption; debug/native reduced-host execution PASS')
