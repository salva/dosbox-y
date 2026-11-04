#!/usr/bin/env python3
"""Owning process checks on reduced DOS memory; no fresh startup claim."""
import os
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'restored-root'
work.mkdir(exist_ok=True)
text = (root / 'src/dos/dos_execute.cpp').read_text()
check = text[text.index('bool RestoredSession::validRoot('):text.index('RestoredSession::Invocation &RestoredSession::invocation()')]
source = r'''
#include "restored_session.h"
#include <cassert>
#include <cstdint>
#include <cstring>
using RealPt=uint32_t;
unsigned char ram[0xa0000]={};
bool dos_kernel_disabled=false;uint16_t shell_psp=0x100;
uint16_t mem_readw(uint32_t p){assert(p+2<=sizeof ram);return ram[p]|(ram[p+1]<<8);}
uint32_t mem_readd(uint32_t p){return mem_readw(p)|(uint32_t(mem_readw(p+2))<<16);}
void word(uint32_t p,uint16_t v){ram[p]=v;ram[p+1]=v>>8;}
void dword(uint32_t p,uint32_t v){word(p,v);word(p+2,v>>16);}
uint16_t RealSeg(RealPt p){return p>>16;}uint16_t RealOff(RealPt p){return p;}
unsigned MEM_ConventionalPages(){return sizeof ram/4096;}
struct {uint16_t firstMCB=0x1ff,current=0x200;uint16_t psp(){return current;}}dos;
struct DOS_MCB {uint32_t p;DOS_MCB(uint16_t s):p(unsigned(s)*16){}
 bool isValid(){return ram[p]=='M'||ram[p]=='Z';}bool isLastMCB(){return ram[p]=='Z';}
 uint16_t GetSize(){return mem_readw(p+3);}uint16_t GetPSPSeg(){return mem_readw(p+1);}};
struct DOS_PSP {uint32_t p;DOS_PSP(uint16_t s):p(unsigned(s)*16){}
 uint16_t GetParent(){return mem_readw(p+0x16);}RealPt GetStack(){return mem_readd(p+0x2e);}};
''' + check + r'''
void setup(){
 memset(ram,0,sizeof ram);dos_kernel_disabled=false;shell_psp=0x100;dos.current=0x200;dos.firstMCB=0x1ff;
 ram[0x1ff0]='Z';word(0x1ff1,0x200);word(0x1ff3,0x100);
 word(0x2000,0x20cd);word(0x1000,0x20cd);word(0x2016,0x100);
 dword(0x202e,0x20000100);
}
int main(){
 setup();assert(RestoredSession::validRoot(0x200,true));
 // Hooks are guest payload: a nondefault but representable INT22 is legitimate.
 dword(0x200a,0xf0000100);assert(RestoredSession::validRoot(0x200,false));
 word(0x2016,0x200);assert(RestoredSession::validRoot(0x200,true));
 assert(RestoredSession::validRoot(0x200,false));
 setup();dos.current=0x300;assert(!RestoredSession::validRoot(0x200,false));
 setup();word(0x1ff1,0x300);assert(!RestoredSession::validRoot(0x200,false));
 setup();dos.firstMCB=0x300;assert(!RestoredSession::validRoot(0x200,false));
 setup();word(0x1ff3,0xffff);assert(!RestoredSession::validRoot(0x200,false));
 setup();word(0x1ff3,15);assert(!RestoredSession::validRoot(0x200,false));
 setup();word(0x2000,0);assert(!RestoredSession::validRoot(0x200,false));
 setup();dword(0x200e,0xffffffff);assert(!RestoredSession::validRoot(0x200,false));
 setup();dword(0x202e,0x2000ffff);assert(!RestoredSession::validRoot(0x200,false));
 setup();shell_psp=0x200;assert(!RestoredSession::validRoot(0x200,true));
 setup();dos_kernel_disabled=true;assert(!RestoredSession::validRoot(0x200,false));
 setup();assert(!RestoredSession::validRoot(0xffff,false));
 // Advance through a prior block in the same bounded increasing chain.
 setup();dos.firstMCB=0x1fd;ram[0x1fd0]='M';word(0x1fd3,1);
 assert(RestoredSession::validRoot(0x200,false));
 ram[0x1fd0]='Z';assert(!RestoredSession::validRoot(0x200,false));
}
'''
path = work / 'process.cpp'
path.write_text(source)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined', '-I'+str(root/'include'), str(path), '-o', str(work/'process')], check=True)
subprocess.run([str(work/'process')], check=True)
print('root SDA/signature/MCB ownership/parent/vector/stack checks passed; reduced memory only')
