#!/usr/bin/env python3
"""Actual LocalFile close body, real FILE flush/close failure and shared refs."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/dos/drive_local.cpp').read_text();a=s.index('bool LocalFile::Close()');b=s.index('\nuint16_t LocalFile::GetInformation',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'local-file-close';w.mkdir(exist_ok=True)
metadata=(root/'src/dos/dos_files.cpp').read_text()
ma=metadata.index('void DOS_File::LoadState(')
mb=metadata.index('\nextern bool dos_kernel_disabled;',ma)
code=r'''
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
#define DOS_SEEK_SET 0
static void E_Exit(const char*) {throw std::runtime_error("fatal owner");}
static uint16_t DOS_PackTime(uint16_t,uint16_t,uint16_t){return 7;}
static uint16_t DOS_PackDate(uint16_t,uint16_t,uint16_t){return 9;}
#include <cstdio>
#include <ctime>
static bool conversion_ok=true;
static tm* probe_localtime(const time_t* value){return conversion_ok?std::localtime(value):nullptr;}
#define localtime probe_localtime
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#define LOG_MSG(...) ((void)0)
struct DOS_File {
 uint32_t flags=0;bool open=true;unsigned attr=0,time=0,date=0;int refCtr=1;uint8_t hdrive=0;
 virtual bool Seek(uint32_t*,int)=0;virtual bool Close()=0;
 void LoadState(std::istream&,bool);
};
struct LocalFile : DOS_File {
 bool newtime=false;FILE*fhandle=nullptr;int last_action=0;
 bool Seek(uint32_t*p,int) override {return fseek(fhandle,*p,SEEK_SET)==0;}
 static constexpr int WRITE=2;int file_access_tries=0;
 bool update_ok=true;bool UpdateLocalDateTime(){return update_ok;};bool Close();uint32_t GetSeekPos();bool UpdateDateTimeFromHost();
};
'''+metadata[ma:mb]+s[a:b]+s[s.index('uint32_t LocalFile::GetSeekPos()'):s.index('LocalFile::LocalFile()')]+s[s.index('bool LocalFile::UpdateDateTimeFromHost'):s.index('bool LocalFile::UpdateLocalDateTime')]+r'''
template<class T>void put(std::ostream&s,T v){s.write(reinterpret_cast<const char*>(&v),sizeof(v));}
int main(){
 for(int refs:{0,1,2}){
  LocalFile f;f.fhandle=tmpfile();assert(f.fhandle);const int fd=fileno(f.fhandle);
  std::ostringstream out;put(out,uint32_t(0));put(out,f.flags);put(out,uint8_t(0));
  put(out,f.attr);put(out,f.time);put(out,f.date);put(out,refs);put(out,f.hdrive);put(out,uint32_t(0));
  std::istringstream in(out.str());f.LoadState(in,false);
  assert(!f.open && !f.fhandle && f.refCtr==refs);
  errno=0;assert(fcntl(fd,F_GETFD)==-1 && errno==EBADF);
 }
 {LocalFile f;f.fhandle=tmpfile();assert(f.fhandle);f.time=3;f.date=4;conversion_ok=false;assert(!f.UpdateDateTimeFromHost());assert(f.time==3 && f.date==4);conversion_ok=true;assert(f.Close());}
 {LocalFile f;assert(f.GetSeekPos()==0xffffffff);assert(!f.UpdateDateTimeFromHost());assert(f.Close());}
 {LocalFile f;f.fhandle=tmpfile();assert(f.UpdateDateTimeFromHost());assert(f.time==7 && f.date==9);assert(f.GetSeekPos()==0);close(fileno(f.fhandle));f.time=3;f.date=4;assert(!f.UpdateDateTimeFromHost());assert(f.time==3 && f.date==4);assert(!f.Close());}
 {LocalFile f;f.fhandle=tmpfile();f.last_action=LocalFile::WRITE;f.update_ok=false;assert(!f.Close());assert(!f.open && !f.fhandle);}
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
