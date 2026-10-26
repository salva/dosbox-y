#!/usr/bin/env python3
"""Required native file reconstruction cannot silently discard a saved handle."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text()
a = s.index('\t\t\tif (file_drive >= DOS_DRIVES', s.index('void POD_Load_DOS_Files'))
b = s.index('\n\t\t}', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-required-file'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <cstdint>
#include <initializer_list>
constexpr unsigned DOS_DRIVES = 2;
void E_Exit(const char*) { throw 1; }
struct File { int loads=0; void LoadState(int&, bool pop) { assert(!pop); ++loads; } };
struct Drive {
 int calls=0; bool result=true; File *prepared=nullptr;
 bool FileOpen(File **out, const char*, uint8_t) { ++calls; *out=prepared; return result; }
};
void load(Drive **Drives, File **Files, unsigned file_drive) {
 unsigned lcv=0; const char *file_name="required"; uint8_t file_flags=0; int stream=0;
SEAM
}
int main() {
 File file; File *files[]={nullptr}; Drive drive; Drive *drives[]={&drive,nullptr};
 for (unsigned index:{1u,2u,255u}) {
  bool fatal=false; try { load(drives,files,index); } catch(int) { fatal=true; }
  assert(fatal && drive.calls==0 && !files[0]);
 }
 for (bool result:{false,true}) {
  drive.result=result; drive.prepared=result ? nullptr : &file;
  bool fatal=false; try { load(drives,files,0); } catch(int) { fatal=true; }
  assert(fatal && file.loads==0);
 }
 drive.result=true; drive.prepared=&file; load(drives,files,0);
 assert(files[0]==&file && file.loads==1 && drive.calls==3);
}
'''.replace('SEAM',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Required native DOS file reconstruction ASan/UBSan PASS; post-install failure, not prepared-resource admission')
