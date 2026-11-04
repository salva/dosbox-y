#!/usr/bin/env python3
"""Execute actual task-state reader; not protected-mode runtime qualification."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'cpu-task-state'
work.mkdir(exist_ok=True)
text=(root/'src/cpu/cpu.cpp').read_text()
a=text.index('void TaskStateSegment::LoadState( std::istream& stream )')
b=text.index('// TODO: This looks to be unused',a)
source=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
struct TaskStateSegment {
 struct {uint64_t saved;}desc;
 uintptr_t selector;uint32_t base;uintptr_t limit,is386;bool valid;
 void LoadState(std::istream&);
};
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
'''+text[a:b]+r'''
std::string wire(uintptr_t kind,unsigned char valid) {
 TaskStateSegment t={};t.desc.saved=42;t.selector=8;t.base=0x100;t.limit=0x67;t.is386=kind;
 std::string s;
 #define PUT(v) s.append(reinterpret_cast<const char*>(&t.v),sizeof(t.v))
 PUT(desc.saved);PUT(selector);PUT(base);PUT(limit);PUT(is386);
 s.append(1,char(valid));return s;
}
int main(){
 for(uintptr_t kind:{0u,1u,8u}) for(unsigned char valid:{0,1}) {
  const auto bytes=wire(kind,valid);
  for(size_t n=0;n<bytes.size();++n){TaskStateSegment t={};std::istringstream s(bytes.substr(0,n));t.LoadState(s);assert(s.fail());}
  TaskStateSegment t={};std::istringstream s(bytes);t.LoadState(s);
  assert(s && t.is386==kind && t.valid==bool(valid) && t.desc.saved==42);
 }
 for(unsigned valid=2;valid<256;++valid){TaskStateSegment t={};std::istringstream s(wire(8,valid));t.LoadState(s);assert(s.fail());}
 for(uintptr_t kind:{2u,3u,7u,9u,~0u}){TaskStateSegment t={};std::istringstream s(wire(kind,1));t.LoadState(s);assert(s.fail());}
}
'''
(work/'test.cpp').write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('actual task-state consumer: truncations/raw bools/kinds 0/1/8 ASan/UBSan passed')
