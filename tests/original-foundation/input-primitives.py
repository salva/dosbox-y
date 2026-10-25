#!/usr/bin/env python3
"""Selected input primitive checks; not integrated input/lifetime qualification."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'input-primitives'
work.mkdir(exist_ok=True)
s=r'''
#include <cassert>
#include <cstddef>
#include "native_input.h"
int main(){
 unsigned index=999;KBD_KEYS value=KBD_NONE;
 assert(!NativeInput::key(nullptr,value));
 assert(!NativeInput::key("unknown",value));
 for(unsigned i=0;i<NativeInput::KeyCount;++i){
  assert(NativeInput::key_index(NativeInput::KeyVocabulary[i].name,index) && index==i);
  assert(NativeInput::key(NativeInput::KeyVocabulary[i].name,value));
  assert(value==NativeInput::KeyVocabulary[i].value);
 }
 NativeInput::Cursor cursor;NativeInput::Point p;
 assert(cursor.map(0,0,10,20,640,480,p));
 assert(p.x==10 && p.y==20 && p.dx==0 && p.dy==0);
 cursor.dispatched(p,10,20);
 assert(cursor.map(65535,65535,10,20,640,480,p));
 assert(p.x==649 && p.y==499 && p.dx==639 && p.dy==479);
 assert(!cursor.map(65536,0,0,0,640,480,p));
 assert(!cursor.map(0,0,-1,0,640,480,p));
 assert(!cursor.map(65535,0,INT32_MAX,0,640,480,p));
 assert(!cursor.map(0,0,0,0,1,480,p));
 assert(!cursor.map(0,0,0,0,4097,480,p));
 cursor.invalidate();assert(cursor.map(32768,32768,0,0,640,480,p));
 assert(p.x==320 && p.y==240 && p.dx==0 && p.dy==0);
}
'''
s += '\nstruct Keyboard {unsigned scanset=1;bool cb_xlat=true,active=true,scanning=true,reset=false;} keyb;\nbool IS_PC98_ARCH=false;int machine=0;constexpr int MCH_PCJR=1;\n'
k=(root/'src/hardware/keyboard.cpp').read_text()
a=k.index('uint64_t KEYBOARD_NativeInputTuple()')
s += k[a:k.index('void KEYBOARD_AddKey(KBD_KEYS',a)]
s += r'''
struct ProfileCheck {ProfileCheck(){
 assert(KEYBOARD_NativeInputProfile());
 const auto initial=KEYBOARD_NativeInputTuple();
 keyb.scanset=2;assert(KEYBOARD_NativeInputTuple()==(initial^3));
 keyb.scanset=1;keyb.reset=true;assert(!KEYBOARD_NativeInputProfile());
 assert(KEYBOARD_NativeInputTuple()==(initial|(1u<<11)));keyb.reset=false;
 keyb.cb_xlat=false;assert(!KEYBOARD_NativeInputProfile());keyb.cb_xlat=true;
 keyb.active=false;assert(!KEYBOARD_NativeInputProfile());keyb.active=true;
 keyb.scanning=false;assert(!KEYBOARD_NativeInputProfile());keyb.scanning=true;
 IS_PC98_ARCH=true;assert(!KEYBOARD_NativeInputProfile());IS_PC98_ARCH=false;
 machine=MCH_PCJR;assert(!KEYBOARD_NativeInputProfile());machine=0;
 assert(KEYBOARD_NativeInputTuple()==initial);
}} profile_check;
'''
(work/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'include'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Native input vocabulary/cursor ASan/UBSan PASS (not frontend integration)')
