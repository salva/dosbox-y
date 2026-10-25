#!/usr/bin/env python3
"""Actual gameport handlers and logical initialization; not SDL/runtime save proof."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'logical-gameport-regression'
work.mkdir(exist_ok=True)
text=(root/'src/hardware/joystick.cpp').read_text()
handlers=text[text.index('#define RANGE'):text.index('void JOYSTICK_Enable')]
init_start=text.index('\t\tif (joytype == JOY_LOGICAL)')
init=text[init_start:text.index('\t\tstick[0].xtick =',init_start)]
source=r'''
#include <cassert>
#include <cstdint>
using Bitu=uintptr_t;
enum JoystickType {JOY_NONE,JOY_LOGICAL};
uint32_t PIC_Ticks=0;
double now=100;
double PIC_FullIndex(){return now;}
'''+handlers+r'''
int main(){
 joytype=JOY_LOGICAL;
 stick[0].xpos=1;stick[1].enabled=true;
'''+init+r'''
 assert(stick[0].enabled && !stick[1].enabled);
 assert(stick[0].xpos==0 && stick[0].ypos==0);
 assert(!stick[0].button[0] && !stick[0].button[1]);
 write_p201(0x201,0,1);
 assert(stick[0].xcount==64 && stick[0].ycount==64);
 for(unsigned i=0;i<64;++i)assert(read_p201(0x201,1)==255);
 assert(read_p201(0x201,1)==252);
 write_p201(0x201,0,1);PIC_Ticks=11;
 assert(read_p201(0x201,1)==252 && !write_active);
 write_p201_timed(0x201,0,1);
 assert(stick[0].xtick>now && stick[0].ytick==stick[0].xtick);
 assert(read_p201_timed(0x201,1)==255);
 now=stick[0].xtick+0.001;
 assert(read_p201_timed(0x201,1)==252);
 // Existing non-centered second-stick/swapped-axis behavior is retained.
 stick[1].enabled=true;stick[1].xpos=-1;stick[1].ypos=1;swap34=true;
 write_p201(0x201,0,1);
 assert(stick[1].xcount==128 && stick[1].ycount==0);
 stick[1].button[1]=true;
 assert(!(read_p201(0x201,1)&128));
}
'''
(work/'test.cpp').write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('actual gameport handlers/logical initialization ASan/UBSan PASS (not SDL/save qualification)')
