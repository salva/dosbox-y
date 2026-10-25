#!/usr/bin/env python3
"""A named system slot mismatch cannot truncate successful owner loading."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/dos/dos_files.cpp').read_text()
a=s.index('\t\t\tif( Files[lcv] && Files[lcv]->GetName() != NULL )',s.index('void POD_Load_DOS_Files'))
b=s.index('\n\n\t\t\t// ignore NULL file',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-system-slot'
w.mkdir(exist_ok=True)
code=r'''
#include <cstring>
#include <cassert>
#include <cstddef>
void E_Exit(const char*){throw 1;}
struct File {const char *name;const char*GetName(){return name;}};
int load(File**Files){int consumed=0;for(int lcv=0;lcv<2;++lcv){
SEAM
++consumed;
}return consumed;}
int main(){
 for(const char *name:{"NUL","CON","LPT1","PRN","AUX","EMMXXXX0"}){
  File f{name};File*files[]={&f,nullptr};bool fatal=false;
  try{load(files);}catch(int){fatal=true;}assert(fatal && files[0]==&f);
 }
 File f{"ordinary"};File*files[]={&f,nullptr};assert(load(files)==2 && files[0]==nullptr);
 File unnamed{nullptr};files[0]=&unnamed;assert(load(files)==2 && files[0]==&unnamed);
}
'''.replace('SEAM',s[a:b]).replace('#include <cstddef>','#include <cstddef>\n#include <initializer_list>')
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('DOS system-slot mismatch fatal ASan/UBSan PASS; not native-device admission')
