#!/usr/bin/env python3
"""Actual DOS close wrapper retires once even when native close failed."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/dos/dos_files.cpp').read_text();a=s.index('bool DOS_CloseFile(');b=s.index('\nbool DOS_FlushFile',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-close-result';w.mkdir(exist_ok=True)
code=r'''
#include <cstdint>
#include <initializer_list>
#include <cassert>
using Bits=int;
constexpr unsigned DOS_FILES=3;constexpr int DOSERR_INVALID_HANDLE=6,DOSERR_ACCESS_DENIED=5;
int error=0,closed=0,deleted=0,retired=0;bool log_fileio=false;
void DOS_SetError(int e){error=e;}
unsigned RealHandle(unsigned e){return e;}
struct File{bool open=true,neverclose=false,result=true;int refs=1;const char*name="x";
 bool IsOpen(){return open;}bool Close(){++closed;open=false;return result;}int RemoveRef(){return --refs;}~File(){++deleted;}};
File*storage[DOS_FILES]={};File**Files=storage;
struct {int psp(){return 0;}}dos;
struct DOS_PSP{explicit DOS_PSP(int){}void SetFileHandle(unsigned,uint8_t){++retired;}};
#define LOG(a,b) printf
#define LOG_FILES 0
#define LOG_NORMAL 0
#include <cstdio>
'''+s[a:b]+r'''
int main(){
 for(bool ok:{false,true})for(bool fcb:{false,true})for(int refs:{1,2}){
  closed=deleted=retired=error=0;File*f=new File;f->refs=refs;f->result=ok;Files[1]=f;uint8_t count=0;
  assert(DOS_CloseFile(1,fcb,&count)==ok);assert(closed==1);assert(retired==!fcb);assert(count==refs);assert(error==(ok?0:DOSERR_ACCESS_DENIED));
  if(refs==1){assert(deleted==1 && !Files[1]);assert(!DOS_CloseFile(1,fcb,&count));assert(closed==1);}
  else{assert(Files[1]==f && f->refs==1);delete f;Files[1]=nullptr;}
 }
 assert(!DOS_CloseFile(DOS_FILES,false,nullptr));
}
'''.replace('#include <cstdint>','#include <cstdint>\n#include <initializer_list>')
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('DOS close actual wrapper result and once-only retirement ASan/UBSan PASS')
