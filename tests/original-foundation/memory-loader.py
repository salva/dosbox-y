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
#include <vector>
#include <memory>
#include <algorithm>
template<class T> void readPOD(std::istream &s,T &v){s.read(reinterpret_cast<char*>(&v),sizeof(v));}
using Bitu=unsigned;using MemHandle=int;struct PageHandler {};
'''+struct+r'''
char bytes[16384];char *MemBase=bytes;size_t MemSize=sizeof(bytes);
PageHandler handler;PageHandler *handlers[4];MemHandle handles[4];
PageHandler *Memory_PageHandler_table[]={nullptr,&handler};
bool dos_kernel_disabled=false;
unsigned binds=0,releases=0;bool fail_bind=false;
void lfb_mem_cb_init(){++binds;if(fail_bind)throw 8;}
void lfb_mem_cb_free(){++releases;}
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
 memory.pages=memory.handler_pages=4;memory.phandlers=handlers;memory.mhandles=handles;
 memory.lfb.handler=memory.lfb_mmio.handler=&handler;
 memory.address_bits=32;memory.mem_alias_pagemask=0xfffff;
 memory.mem_alias_pagemask_active=memory.mem_alias_pagemask;
 Loader loader;MemoryBlock original=memory;
 for(unsigned count:{5u,0x40001u}){memory.pages=count;std::ostringstream out;
 loader.getBytes(out);assert(!out && out.str().empty());memory=original;}
 std::ostringstream valid;loader.getBytes(valid);assert(valid && !valid.str().empty());
 auto hostile=original;hostile.pages=0xffffffff;
 auto reported=original;reported.reported_pages=original.pages+1;
 auto conventional=original;conventional.reported_pages=1;
 auto bits=original;bits.address_bits=40;
 auto mask=original;mask.mem_alias_pagemask=0xffffffff;
 auto active=original;active.mem_alias_pagemask_active ^= 0x200;
 auto assignment=original;assignment.hw_next_assign^=4096;
 auto high=original;high.reported_pages_4gb=1;
 auto span=original;span.lfb.start_page=0xfffff;span.lfb.end_page=0;span.lfb.pages=1;
 auto extent=original;extent.lfb_mmio.end_page=2;extent.lfb_mmio.pages=1;
 for(auto h:{hostile,reported,conventional,bits,mask,active,assignment,high,span,extent}) {std::istringstream in(archive(h));bool fatal=false;
 try{loader.setBytes(in);}catch(int){fatal=true;}assert(fatal && memory.pages==4);}
 auto encoded=original;encoded.phandlers=nullptr;encoded.mhandles=nullptr;
 for(unsigned char value:{2,127,255}) {
  auto wire=archive(original);
  const auto offset=reinterpret_cast<const char*>(&original.a20.enabled)-reinterpret_cast<const char*>(&original);
  wire[size_t(offset)]=char(value);std::istringstream in(wire);bool fatal=false;
  try{loader.setBytes(in);}catch(int){fatal=true;}assert(fatal);
 }
 encoded.lfb.handler=encoded.lfb_mmio.handler=nullptr;
 {auto masked=encoded;masked.mem_alias_pagemask_active &= ~0x100u;
 std::istringstream in(archive(masked));loader.setBytes(in);assert(in);
 assert(memory.mem_alias_pagemask_active==masked.mem_alias_pagemask_active);}
 std::istringstream good(archive(encoded));loader.setBytes(good);
 assert(good && memory.phandlers==handlers && memory.mhandles==handles);
 assert(memory.lfb.handler==&handler && memory.lfb_mmio.handler==&handler);
 assert(handlers[0]==&handler && handlers[1]==&handler);
 {auto mapped=encoded;mapped.lfb.start_page=0x100;mapped.lfb.end_page=0x110;mapped.lfb.pages=16;
  mapped.lfb_mmio.start_page=0x200;mapped.lfb_mmio.end_page=0x210;mapped.lfb_mmio.pages=16;
  const auto before=binds;
  memory.lfb.handler=nullptr;bool absent=false;std::istringstream missing(archive(mapped));
  try{loader.setBytes(missing);}catch(int){absent=true;}
  assert(absent && binds==before);memory.lfb.handler=&handler;
  memory.lfb_mmio.handler=nullptr;absent=false;std::istringstream missing_mmio(archive(mapped));
  try{loader.setBytes(missing_mmio);}catch(int){absent=true;}
  assert(absent && binds==before);memory.lfb_mmio.handler=&handler;
  std::istringstream in(archive(mapped));loader.setBytes(in);
  assert(in && binds==before+1 && memory.lfb.start_page==0x100);
  fail_bind=true;bool failed=false;std::istringstream failure(archive(mapped));
  try{loader.setBytes(failure);}catch(int code){failed=code==8;}
  assert(failed && memory.lfb.start_page==0x100);fail_bind=false;
  const auto released=releases;std::istringstream disabled(archive(encoded));loader.setBytes(disabled);
  assert(disabled && releases==released+1);
 }
 for(unsigned n:{0u,1u,unsigned(sizeof(MemoryBlock)-1),unsigned(sizeof(MemoryBlock)+1)}) {
  std::istringstream in(archive(encoded).substr(0,n));bool fatal=false;
  try{loader.setBytes(in);}catch(int){fatal=true;}assert(fatal);
 }
 for(int next:{-2,1,4,2147483647}) {
  handles[1]=next;std::istringstream in(archive(encoded));bool fatal=false;
  try{loader.setBytes(in);}catch(int){fatal=true;}assert(fatal);
 }
 handles[1]=2;handles[2]=3;handles[3]=1;
 {std::istringstream in(archive(encoded));bool fatal=false;
 try{loader.setBytes(in);}catch(int){fatal=true;}assert(fatal);}
 handles[3]=-1;std::istringstream chain(archive(encoded));loader.setBytes(chain);assert(chain);
 handles[1]=3;handles[2]=3; // Shared tails are acyclic, not validated allocation ownership.
 {std::istringstream in(archive(encoded));loader.setBytes(in);assert(in);}
 handles[1]=handles[2]=handles[3]=0;
 std::istringstream bad(archive(encoded,200));bool fatal=false;
 try{loader.setBytes(bad);}catch(int){fatal=true;}assert(fatal);
}
'''
(w/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual memory loader extent/native pointer/handler/truncation ASan/UBSan PASS; not semantic admission')
