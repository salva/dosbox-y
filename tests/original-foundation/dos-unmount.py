#!/usr/bin/env python3
"""Actual save-loader unmount: native refusal is fatal, not successful install."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text()
a = s.index('void unmount(int lcv) {')
b = s.index('\nvoid POD_Load_DOS_Files', a)
c = s.index('            if (!Drives[lcv])\n                E_Exit("Cannot restore save-state DOS drive: native reconstruction failed");', b)
d = s.index('            Drives[lcv]->LoadState(stream);', c) + len('            Drives[lcv]->LoadState(stream);')
e = s.index('                if (Drives[lcv] && lcv != ZDRIVE_NUM) unmount(lcv);', b)
f = s.index('\n', e)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-unmount'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <stdexcept>
constexpr int DOS_DRIVES=26;
struct Drive {virtual ~Drive(){};int loads=0;void LoadState(int){++loads;}};
struct isoDrive:Drive{};
Drive *Drives[DOS_DRIVES]={};
int result=0,calls=0,detaches=0,menus=0,writes=0;
namespace DriveManager {int UnmountDrive(int){++calls;return result;}}
void E_Exit(const char *s){throw std::runtime_error(s);}
void IDE_CDROM_Detach(int){++detaches;}
void DOS_EnableDriveMenu(char){++menus;}
struct {struct {int mediaid=0,dpb_size=1;} tables;} dos;
int Real2Phys(int n){return n;}
void mem_writeb(int,int){++writes;}
BODY
void consume(int lcv){int stream=0;CONSUME}
void absent(int lcv,int ZDRIVE_NUM){ABSENT}
int main(){
 bool fatal=false;try{consume(2);}catch(const std::runtime_error&){fatal=true;}
 assert(fatal);
 Drive ordinary;isoDrive cd;
 Drives[2]=&ordinary;consume(2);assert(ordinary.loads==1);Drives[2]=nullptr;
 for(int slot:{-1,DOS_DRIVES}){
  bool fatal=false;try{unmount(slot);}catch(const std::runtime_error&){fatal=true;}
  assert(fatal && calls==0);
 }
 unmount(2);assert(calls==0);
 for(Drive *drive:{&ordinary,static_cast<Drive*>(&cd)}){
  Drives[2]=drive;result=1;bool fatal=false;
  int before=calls;
  try{unmount(2);}catch(const std::runtime_error&){fatal=true;}
  assert(fatal && calls==before+1 && Drives[2]==drive);
  assert(detaches==0 && menus==0 && writes==0);
 }
 result=0;unmount(2);
 assert(!Drives[2] && detaches==1 && menus==1 && writes==1);
 Drives[2]=&ordinary;unmount(2);
 assert(!Drives[2] && detaches==1 && menus==2 && writes==2);
 Drives[DOS_DRIVES-1]=&ordinary;unmount(DOS_DRIVES-1);
 assert(!Drives[DOS_DRIVES-1] && menus==3 && writes==3);
 // A relocated system drive is protected by identity, not the last-slot index.
 Drives[3]=&ordinary;absent(3,3);assert(Drives[3]==&ordinary && menus==3);
 Drives[DOS_DRIVES-1]=&cd;absent(DOS_DRIVES-1,3);
 assert(!Drives[DOS_DRIVES-1] && menus==4 && detaches==2);
 Drives[2]=&ordinary;absent(2,DOS_DRIVES-1);
 assert(!Drives[2] && menus==5);
}
'''.replace('BODY',s[a:b]).replace('CONSUME',s[c:d]).replace('ABSENT',s[e:f])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual loader native-unmount refusal/finalization ASan/UBSan PASS; not prepared drive/resource qualification')
