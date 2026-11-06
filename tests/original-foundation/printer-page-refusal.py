#!/usr/bin/env python3
"""Actual newPage owner body, refused acquisition retains all pending state."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
s=(root/'src/hardware/parport/printer.cpp').read_text()
a=s.index('void CPrinter::newPage('); b=s.index('\nvoid CPrinter::printChar',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'printer-page-refusal';w.mkdir(exist_ok=True)
p=w/'probe.cpp'
p.write_text('''#include <cassert>
using Bitu=unsigned;
struct SDL_Rect{int x,y,w,h;};struct Surface{int w=20,h=30;void* format=nullptr;};
unsigned removes=0,fills=0,attempts=0;bool printer_timout=true;
void PRINTER_EventHandler(Bitu){}
void PIC_RemoveEvents(void(*)(Bitu)){++removes;}
int SDL_MapRGB(void*,int,int,int){return 0;}
void SDL_FillRect(Surface*,SDL_Rect*,int){++fills;}
struct CPrinter{Surface*page=nullptr;bool timeout_dirty=true,accept=false;
 double curX=4,curY=5,leftMargin=1,topMargin=2;
 bool outputPage(){++attempts;return accept;}void newPage(bool,bool);};
'''+s[a:b]+'''
int main(){CPrinter p;p.newPage(true,true);assert(!attempts&&!removes&&!fills);
 Surface s;p.page=&s;p.newPage(true,true);
 assert(attempts==1&&!removes&&!fills&&p.timeout_dirty&&p.curX==4&&p.curY==5);
 p.accept=true;p.newPage(true,true);
 assert(attempts==2&&removes==1&&fills==1&&!p.timeout_dirty&&p.curX==1&&p.curY==2);
 p.curX=7;p.curY=8;p.newPage(false,false);assert(attempts==2&&removes==2&&fills==2&&p.curX==7&&p.curY==2);
}
''')
subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p),'-o',str(w/'probe')],check=True)
subprocess.run([str(w/'probe')],check=True)
print('Actual newPage null/refused/success/reset branches ASan/UBSan PASS')
