#!/usr/bin/env python3
"""Consumed DOS file metadata seam; not native host-file ownership qualification."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-file-metadata'
w.mkdir(exist_ok=True)
s=(root/'src/dos/dos_files.cpp').read_text()
a=s.index('void DOS_File::LoadState(')
b=s.index('\nextern bool dos_kernel_disabled;',a)
code=r'''
#include <algorithm>
#include <sstream>
#include <cassert>
#include <cstdint>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
#define DOS_SEEK_SET 0
void E_Exit(const char*) {throw 1;}
struct DOS_File {
 uint32_t flags=9;bool open=true;uint16_t attr=1,time=2,date=3;
 long refCtr=4;uint8_t hdrive=5;int seeks=0,closes=0;bool ok=true;
 bool Seek(uint32_t*,int){++seeks;return ok;}
 long close_refs=-1;bool Close(){++closes;close_refs=refCtr;return ok;}
 void LoadState(std::istream&,bool);
};
template<class T>void put(std::ostream&s,T v){s.write(reinterpret_cast<char*>(&v),sizeof(v));}
std::string archive(uint8_t opened=1,long refs=2){std::ostringstream s;put(s,uint32_t(3));s<<"abc";
 put(s,uint32_t(9));put(s,opened);put(s,uint16_t(7));put(s,uint16_t(8));put(s,uint16_t(9));
 put(s,refs);put(s,uint8_t(3));put(s,uint32_t(42));return s.str();}
SEAM
int main(){auto bytes=archive();
 for(size_t n=0;n<bytes.size();++n){DOS_File f;std::istringstream s(bytes.substr(0,n));bool fatal=false;
 try{f.LoadState(s,false);}catch(int){fatal=true;}assert(fatal && f.flags==9 && !f.seeks && !f.closes);}
 for(bool pop:{false,true}){DOS_File f;std::istringstream s(bytes);f.LoadState(s,pop);
 assert(s && f.flags==9 && f.seeks==(pop?0:1));}
 for(uint8_t opened:{uint8_t(0),uint8_t(2)}){DOS_File f;std::istringstream s(archive(opened));bool fatal=false;
 try{f.LoadState(s,false);}catch(int){fatal=true;}assert(fatal==(opened==2));assert(f.closes==(opened==0));}
 {DOS_File f;f.ok=false;std::istringstream s(bytes);bool fatal=false;
 try{f.LoadState(s,false);}catch(int){fatal=true;}assert(fatal && f.flags==9 && f.seeks==1);}
 for(bool pop:{false,true})for(uint8_t opened:{uint8_t(0),uint8_t(1)}){
 DOS_File f;f.flags=12;std::istringstream s(archive(opened));bool fatal=false;
 try{f.LoadState(s,pop);}catch(int){fatal=true;}
 assert(fatal==!pop && f.flags==12 && f.refCtr==4 && !f.seeks && !f.closes);}
 for(bool pop:{false,true})for(long refs:{-1L,0L}){DOS_File f;std::istringstream s(archive(1,refs));bool fatal=false;
 try{f.LoadState(s,pop);}catch(int){fatal=true;}assert(fatal && f.flags==9 && f.refCtr==4 && !f.seeks && !f.closes);}
 for(long refs:{0L,1L,2L}){DOS_File f;std::istringstream s(archive(0,refs));f.LoadState(s,false);assert(f.refCtr==refs && f.closes==1 && f.close_refs==1 && !f.seeks);}
 {DOS_File f;f.ok=false;std::istringstream s(archive(0,2));bool fatal=false;
 try{f.LoadState(s,false);}catch(int){fatal=true;}assert(fatal && f.closes==1 && f.close_refs==1 && f.refCtr==2);}
 {DOS_File f;std::ostringstream out;put(out,uint32_t(0xffffffff));std::istringstream s(out.str());bool fatal=false;
 try{f.LoadState(s,false);}catch(int){fatal=true;}assert(fatal && !f.seeks);}
}
'''.replace('SEAM',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual DOS metadata truncation/discard/invalid bool/seek failure ASan/UBSan PASS; not native file resources')
