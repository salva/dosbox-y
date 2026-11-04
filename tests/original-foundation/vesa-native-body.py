#!/usr/bin/env python3
"""Actual VBE constructor-body predicate with bounded RAM/callback doubles."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vesa-native-body';w.mkdir(exist_ok=True)
t=(root/'src/ints/int10_vesa.cpp').read_text()
a=t.index('static PhysPt native_interface_start')
b=t.index('void CALLBACK_DeAllocate',a)
c=t.index('bool VESA_NativeInterfaceValid()')
d=t.index('void INT10_SetupVESA',c)
code=r'''
#include <cassert>
#include <cstdint>
#include <stddef.h>
#include <vector>
using Bitu=unsigned;using PhysPt=unsigned;
constexpr unsigned CB_MAX=8;
unsigned char ram[256]={};
uint8_t phys_readb(PhysPt p){assert(p<256);return ram[p];}
Bitu VESA_SetWindow(){return 0;} Bitu VESA_PMSetWindow(){return 0;}
Bitu VESA_PMSetStart(){return 0;} Bitu VESA_PMSetPalette(){return 0;}
Bitu other(){return 0;}
Bitu(*CallBack_Handlers[CB_MAX])()={};
struct {Bitu rmWindow=0,pmWindow=0,pmStart=0,pmPalette=0;} callback;
struct {struct {unsigned pmode_interface=0;}rom;}int10;
'''+t[a:b]+t[c:d]+r'''
int main(){
 assert(VESA_NativeInterfaceValid());
 callback.rmWindow=1;assert(!VESA_NativeInterfaceValid());
 CallBack_Handlers[1]=VESA_SetWindow;
 VESA_SetupNativeInterfaceBody(16,80);
 assert(VESA_NativeInterfaceValid());
 int10.rom.pmode_interface=32;
 callback.pmWindow=2;callback.pmStart=3;callback.pmPalette=4;
 CallBack_Handlers[2]=VESA_PMSetWindow;CallBack_Handlers[3]=VESA_PMSetStart;
 CallBack_Handlers[4]=VESA_PMSetPalette;
 assert(VESA_NativeInterfaceValid());
 for(unsigned n=16;n<80;++n){ram[n]^=1;assert(!VESA_NativeInterfaceValid());ram[n]^=1;}
 ram[15]=1;ram[80]=1;assert(VESA_NativeInterfaceValid()); // only owned extent
 for(unsigned n=1;n<5;++n){auto old=CallBack_Handlers[n];CallBack_Handlers[n]=other;assert(!VESA_NativeInterfaceValid());CallBack_Handlers[n]=old;}
 callback.pmStart=CB_MAX;assert(!VESA_NativeInterfaceValid());callback.pmStart=3;
 int10.rom.pmode_interface=0;assert(!VESA_NativeInterfaceValid());
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual VBE native predicate: every controlled byte, four roles, slot bounds/absent interface under ASan/UBSan (reduced doubles)')
