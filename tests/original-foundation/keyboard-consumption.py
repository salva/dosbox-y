#!/usr/bin/env python3
"""Reduced-layout actual keyboard codec test; not full-device restore proof."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'keyboard-consumption'
work.mkdir(exist_ok=True)
keyboard=(root/'src/hardware/keyboard.cpp').read_text()
header=(root/'include/dosbox.h').read_text()
codec=header[header.index('class SerializeGlobalPOD :'):header.index('//---------------- inline implementation')]
start=keyboard.index('class SerializeKeyboard :')
end=keyboard.index('} dummy;',start)+len('} dummy;')
struct_start=keyboard.index('enum AuxCommands')
struct_end=keyboard.index('} keyb;',struct_start)+len('} keyb;')
source=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
using Bitu=uintptr_t;
enum KBD_KEYS {KBD_NONE,KBD_a,KBD_b,KBD_LAST};
#define KEYBUFSIZE 96
uint8_t port_61_data=0;
class SaveState {
public:
 struct Component { virtual ~Component()=default;
 virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0; };
 static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component&){}
};
'''+codec+'\n'+keyboard[struct_start:struct_end]+'\n'+keyboard[start:end]+r'''
std::string save() {std::ostringstream o;static_cast<SaveState::Component&>(dummy).getBytes(o);return o.str();}
bool load(const std::string& bytes) {std::istringstream i(bytes);dummy.setBytes(i);return bool(i);}
int main() {
 keyb.scanset=1;keyb.pending_key=-1;
 keyb.command=CMD_SETOUTPORT;keyb.p60data=0xa5;keyb.p60changed=true;
 keyb.buf8042_len=2;keyb.buf8042_pos=1;keyb.buf8042[1]=0xfa;
 keyb.ps2mouse.type=MOUSE_INTELLIMOUSE;keyb.ps2mouse.mode=MM_STREAM;
 keyb.ps2mouse.acx=1.5f;keyb.cb_irq12=true;
 auto bytes=save();
 memset(&keyb,0,sizeof(keyb));assert(load(bytes));
 assert(keyb.command==CMD_SETOUTPORT && keyb.p60data==0xa5 && keyb.p60changed);
 assert(keyb.buf8042_len==2 && keyb.buf8042_pos==1 && keyb.buf8042[1]==0xfa);
 assert(keyb.cb_irq12 && keyb.ps2mouse.acx==1.5f && keyb.ps2mouse.mode==MM_STREAM);
 for(size_t n=0;n<bytes.size();++n) assert(!load(bytes.substr(0,n)));
 assert(load(bytes));keyb.buf8042_len=9;assert(!load(save()));
 assert(load(bytes));keyb.buf8042_pos=2;assert(!load(save()));
 assert(load(bytes));keyb.buf8042_pos=3;assert(!load(save()));
 assert(load(bytes));keyb.used=97;assert(!load(save()));
 assert(load(bytes));keyb.pos=96;assert(!load(save()));
 assert(load(bytes));keyb.scanset=0;assert(!load(save()));
 assert(load(bytes));keyb.ps2mouse.resolution=4;assert(!load(save()));
 assert(load(bytes));keyb.ps2mouse.acx=1e30f;assert(!load(save()));
 assert(load(bytes));keyb.ps2mouse.acx=INFINITY;assert(!load(save()));
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Keyboard codec: D1 command/output latch/AUX queue roundtrip, all truncations and field bounds passed')
