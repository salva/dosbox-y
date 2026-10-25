#!/usr/bin/env python3
"""Consumed memory loader extents/handler IDs; not semantic admission."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'memory-loader';w.mkdir(exist_ok=True)
t=(root/'src/hardware/memory.cpp').read_text()
a=t.index('static struct MemoryBlock {');b=t.index('\nuint32_t MEM_get_address_bits',a)
struct=t[a:b]
a=t.index('\tvoid setBytes(std::istream& stream) override',t.index('class SerializeMemory'))
b=t.index('\n} dummy;',a)
method=t[a:b].replace(' override','')
a=t.index('\tvoid getBytes(std::ostream& stream) override',t.index('class SerializeMemory'))
b=t.index('\tvoid setBytes(std::istream& stream)',a)
save=t[a:b].replace(' override','')
s=r'''
#include <cstdint>
#include <cstddef>
#include <sstream>
#include <cassert>
#include <cstring>
using Bitu=unsigned;using MemHandle=int;struct PageHandler {};
'''+struct+r'''
char bytes[8192];char *MemBase=bytes;size_t MemSize=sizeof(bytes);
PageHandler handler;PageHandler *handlers[2];MemHandle handles[2];
PageHandler *Memory_PageHandler_table[]={nullptr,&handler};
bool dos_kernel_disabled=false;
struct SerializeGlobalPOD {static void setBytes(std::istream&) {} static void getBytes(std::ostream&) {}};
[[noreturn]] void E_Exit(const char*) {throw 7;}
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
#define READ_POD_SIZE(p,n) stream.read(reinterpret_cast<char*>(p),n)
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define WRITE_POD_SIZE(p,n) stream.write(reinterpret_cast<const char*>(p),n)
struct Loader {
'''+save+method+r'''
};
std::string archive(MemoryBlock m,unsigned char id=1) {
 std::ostringstream out;out.write(reinterpret_cast<char*>(&m),sizeof(m));
 out.write(bytes,sizeof(bytes));out.write(reinterpret_cast<char*>(handles),sizeof(handles));
 std::string ids(0x40000, char(id));out.write(ids.data(),ids.size());return out.str();
}
int main(){
 memory.pages=memory.handler_pages=2;memory.phandlers=handlers;memory.mhandles=handles;
 memory.lfb.handler=memory.lfb_mmio.handler=&handler;
 Loader loader;MemoryBlock original=memory;
 for(unsigned count:{3u,0x40001u}){memory.pages=count;std::ostringstream out;
 loader.getBytes(out);assert(!out && out.str().empty());memory=original;}
 std::ostringstream valid;loader.getBytes(valid);assert(valid && !valid.str().empty());
 auto hostile=original;hostile.pages=0xffffffff;
 for(auto h:{hostile}) {std::istringstream in(archive(h));bool fatal=false;
 try{loader.setBytes(in);}catch(int){fatal=true;}assert(fatal && memory.pages==2);}
 auto encoded=original;encoded.phandlers=nullptr;encoded.mhandles=nullptr;
 encoded.lfb.handler=encoded.lfb_mmio.handler=nullptr;
 std::istringstream good(archive(encoded));loader.setBytes(good);
 assert(good && memory.phandlers==handlers && memory.mhandles==handles);
 assert(memory.lfb.handler==&handler && memory.lfb_mmio.handler==&handler);
 assert(handlers[0]==&handler && handlers[1]==&handler);
 for(unsigned n:{0u,1u,unsigned(sizeof(MemoryBlock)-1),unsigned(sizeof(MemoryBlock)+1)}) {
  std::istringstream in(archive(encoded).substr(0,n));bool fatal=false;
  try{loader.setBytes(in);}catch(int){fatal=true;}assert(fatal);
 }
 std::istringstream bad(archive(encoded,200));bool fatal=false;
 try{loader.setBytes(bad);}catch(int){fatal=true;}assert(fatal);
}
'''
(w/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual memory loader extent/native pointer/handler/truncation ASan/UBSan PASS; not semantic admission')
