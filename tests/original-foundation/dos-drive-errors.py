#!/usr/bin/env python3
"""Actual consumed CD/PhysFS error branches, not native constructor qualification."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text()
a = s.index('                    if (error && error != 5) {')
b = s.index('                    if (Drives[lcv]) {', a)
c = s.index('                    if (error) {', b)
d = s.index('                    if (Drives[lcv]) {', c)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-drive-errors'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <stdexcept>
int retired=0;
struct Drive {~Drive(){++retired;}};
Drive *Drives[1];
void E_Exit(const char *s){throw std::runtime_error(s);}
void cd(int error){int lcv=0;CD}
void physfs(int error){int lcv=0;PHYSFS}
int main(){
 for(int error:{0,1,2,3,4,5,10}){
  Drives[0]=new Drive;bool fatal=false;int before=retired;
  try{cd(error);}catch(const std::runtime_error&){fatal=true;}
  assert(fatal==(error!=0 && error!=5));
  if(fatal)assert(!Drives[0] && retired==before+1);
  else {assert(Drives[0] && retired==before);delete Drives[0];}
  Drives[0]=new Drive;fatal=false;before=retired;
  try{physfs(error);}catch(const std::runtime_error&){fatal=true;}
  assert(fatal==(error!=0));
  if(fatal)assert(!Drives[0] && retired==before+1);
  else {assert(Drives[0] && retired==before);delete Drives[0];}
 }
}
'''.replace('CD',s[a:b]).replace('PHYSFS',s[c:d])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual CD/PhysFS constructor-error consumption ASan/UBSan PASS; not native drive qualification')
