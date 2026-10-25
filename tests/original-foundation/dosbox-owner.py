#!/usr/bin/env python3
"""Exercise original Dosbox owner error propagation, not a unified save join."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
source = (root/'src/dosbox.cpp').read_text()
a = source.index('class SerializeDosbox :')
b = source.index('} dummy;', a)
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dosbox-owner'
work.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <sstream>
struct SerializeGlobalPOD {
 SerializeGlobalPOD(const char*) {}
 virtual void getBytes(std::ostream&) {}
 virtual void setBytes(std::istream&) {}
};
int ticksRemain=11,ticksLast=12,ticksAdded=13,ticksDone=14,ticksScheduled=15;
int GetTicks(){return 42;}
void POD_Save_Sdlmain(std::ostream &out){out.put('x');}
void POD_Load_Sdlmain(std::istream &in){char c;in.get(c);}
''' + source[a:b] + r'''};
int main(){
 SerializeDosbox owner;SerializeGlobalPOD &entry=owner;
 std::istringstream empty;entry.setBytes(empty);
 assert(empty.fail());
 assert(ticksRemain==11 && ticksLast==12 && ticksAdded==13 && ticksDone==14 && ticksScheduled==15);
 std::istringstream good("x");entry.setBytes(good);
 assert(good && ticksRemain==5 && ticksLast==42 && !ticksAdded && !ticksDone && !ticksScheduled);
}
'''
(work/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Original Dosbox owner failed SDL read leaves host scheduler unchanged: ASan/UBSan PASS; not save qualification')
