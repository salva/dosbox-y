#!/usr/bin/env python3
"""Actual Dosbox-x owner preserves pacing; not a fresh held runtime proof."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dosbox-pacing';w.mkdir(exist_ok=True)
t=(root/'src/dosbox.cpp').read_text()
a=t.index('class SerializeDosbox :');b=t.index('} dummy;',a)
owner=t[a:b]+'};'
pa=t.index('static bool fresh_pacing_pending = false;')
pb=t.index('static Bitu Normal_Loop(void)',pa)
pacing=t[pa:pb]
s=r'''
#include <sstream>
#include <cassert>
struct SerializeGlobalPOD {
 SerializeGlobalPOD(const char*){}
 virtual void getBytes(std::ostream&){}
 virtual void setBytes(std::istream&){}
};
unsigned ticksRemain=13,ticksLast=42,ticksAdded=7,ticksDone=9,ticksScheduled=11;
unsigned clocks=0;unsigned GetTicks(){++clocks;return 123;}
void POD_Save_Sdlmain(std::ostream &s){s.put('x');}
void POD_Load_Sdlmain(std::istream &s){char c;if(!s.get(c)||c!='x')s.setstate(std::ios::failbit);}
unsigned ticksRemainSpeedFrac=3;
'''+pacing+owner+r'''
void unchanged(){assert(ticksRemain==13 && ticksLast==42 && ticksAdded==7 &&
 ticksDone==9 && ticksScheduled==11 && clocks==0);}
int main(){SerializeDosbox owner;auto &base=static_cast<SerializeGlobalPOD&>(owner);
 std::istringstream good("x");base.setBytes(good);assert(good);unchanged();
 std::istringstream bad("");base.setBytes(bad);assert(!bad);unchanged();
 std::ostringstream out;base.getBytes(out);assert(out.str()=="x");unchanged();
 DOSBOX_HoldFreshPacing();unchanged();
 DOSBOX_ContinueFreshPacing();assert(clocks==1 && ticksLast==123);
 assert(!ticksRemain && !ticksRemainSpeedFrac && !ticksAdded && !ticksDone && !ticksScheduled);
 DOSBOX_ContinueFreshPacing();assert(clocks==1);
}
'''
p=w/'test.cpp';p.write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual Dosbox-x codec preserves pacing; hold does not sample clock; explicit continuation rebases host pacing exactly once')
