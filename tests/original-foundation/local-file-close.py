#!/usr/bin/env python3
"""Actual LocalFile close body, real FILE flush/close failure and shared refs."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/dos/drive_local.cpp').read_text();a=s.index('bool LocalFile::Close()');b=s.index('\nuint16_t LocalFile::GetInformation',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'local-file-close';w.mkdir(exist_ok=True)
code=r'''
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#define LOG_MSG(...) ((void)0)
struct LocalFile {
 bool newtime=false,open=true;FILE*fhandle=nullptr;int last_action=0,refCtr=1;
 static constexpr int WRITE=2;unsigned time=0,date=0;
 void UpdateLocalDateTime(){};bool Close();
};
'''+s[a:b]+r'''
int main(){
 {LocalFile f;f.fhandle=tmpfile();assert(f.fhandle);assert(f.Close());assert(!f.open && !f.fhandle);assert(f.Close());}
 {LocalFile f;f.fhandle=tmpfile();f.refCtr=2;assert(f.Close());assert(f.open && f.fhandle);f.refCtr=1;assert(f.Close());}
 {LocalFile f;f.fhandle=fopen("/dev/full","w");assert(f.fhandle);assert(fputs("pending",f.fhandle)>=0);assert(!f.Close());assert(!f.open && !f.fhandle);assert(f.Close());}
 {LocalFile f;f.fhandle=fopen("/dev/full","w");assert(f.fhandle);f.newtime=true;assert(fputs("pending",f.fhandle)>=0);assert(!f.Close());assert(!f.open && !f.fhandle);}
 {LocalFile f;f.fhandle=tmpfile();assert(f.fhandle);close(fileno(f.fhandle));assert(!f.Close());assert(!f.open && !f.fhandle);}
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual LocalFile close real FILE /dev/full/shared-ref ASan/UBSan PASS')
