#!/usr/bin/env python3
"""Actual timestamp body must not retain libc shared tm across callbacks."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/dos/drive_local.cpp').read_text();a=s.index('bool LocalFile::UpdateLocalDateTime');b=s.index('\n\nvoid LocalFile::Flush',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'local-file-time';w.mkdir(exist_ok=True)
code=r'''
#include <cstdint>
#include <ctime>
#include <cassert>
#include <cstring>
uint16_t reg_ax=0,reg_cx=0,reg_dx=0;
#define reg_ah (*reinterpret_cast<unsigned char*>(&reg_ax+0)+1)
// Explicit byte proxies avoid dependence on the native register union.
struct Byte {uint16_t &word;int shift;operator unsigned()const{return (word>>shift)&255;}Byte&operator=(unsigned x){word=(word&~(255<<shift))|(x<<shift);return *this;}};
#undef reg_ah
Byte reg_ah{reg_ax,8},reg_ch{reg_cx,8},reg_cl{reg_cx,0},reg_dh{reg_dx,8},reg_dl{reg_dx,0};
int mode=0,calls=0;tm shared={};
tm* test_localtime(const time_t*){if(mode==1 || (mode==2 && calls==2))return nullptr;if(calls==2)return &shared;shared={};shared.tm_year=100;shared.tm_mon=1;shared.tm_mday=1;return &shared;}
time_t test_mktime(tm*t){assert(t->tm_year==126 && t->tm_mon==9 && t->tm_mday==25);assert(t->tm_hour==12 && t->tm_min==34 && t->tm_sec==56);if(mode==3)return -1;shared=*t;return 10;}
void CALLBACK_RunRealInt(int n){assert(n==0x21);++calls;memset(&shared,0,sizeof(shared));if(calls==1){reg_cx=2026;reg_dh=10;reg_dl=25;}else{reg_ch=12;reg_cl=34;reg_dh=56;}}
struct LocalFile{unsigned time=3,date=4;bool newtime=false;bool UpdateLocalDateTime();};
#define localtime test_localtime
#define mktime test_mktime
'''+s[a:b]+r'''
int main(){for(mode=0;mode<4;++mode){calls=0;reg_ax=7;reg_cx=8;reg_dx=9;LocalFile f;bool ok=f.UpdateLocalDateTime();assert(ok==(mode==0));assert(calls==(mode==1?0:2));assert(reg_ax==7&&reg_cx==8&&reg_dx==9);if(!ok)assert(f.time==3&&f.date==4&&!f.newtime);}}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual timestamp localtime storage and failure preservation ASan/UBSan PASS; callbacks stubbed')
