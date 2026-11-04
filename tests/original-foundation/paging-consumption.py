#!/usr/bin/env python3
"""Exercise the actual paging wire consumer; not a fresh-restore runtime test."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'paging-consumption'
work.mkdir(exist_ok=True)
text = (root / 'src/cpu/paging.cpp').read_text()
start = text.index('void POD_Load_CPU_Paging(std::istream& stream)')
begin = text.index('{', start)
end, depth = begin + 1, 1
while depth:
    depth += (text[end] == '{') - (text[end] == '}')
    end += 1
source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
constexpr unsigned PF_QUEUESIZE=80;
struct Paging {
 uint32_t cr3,cr2;
 struct {uint32_t page,addr;} base;
 struct {uintptr_t read[4],write[4];uint32_t phys_page[4];} tlb;
 struct {uint32_t used,entries[4];} links;
 uint32_t firstmb[272]; bool enabled;
} paging;
using Bitu=uintptr_t;
struct PF_Entry {Bitu cs,eip,page_addr,mpl;};
struct Faults {Bitu used; PF_Entry entries[PF_QUEUESIZE];} pf_queue;
unsigned rebuilds=0;
void PAGING_InitTLB() {
 ++rebuilds; std::memset(&paging.tlb,0,sizeof(paging.tlb)); paging.links.used=0;
}
template<class T> void readPOD(std::istream& s,T& v) {s.read((char*)&v,sizeof(v));}
template<class T> void put(std::ostream& s,const T& v) {s.write((const char*)&v,sizeof(v));}
#define WRITE_POD(p,v) put(stream,v)
'''
start_save = text.index('void POD_Save_CPU_Paging( std::ostream& stream )')
source += text[start_save:start].split('// save state support',1)[0]
source += text[start:end]
source += r'''
std::string wire(unsigned char enabled=1,unsigned faults=0,bool badbase=false) {
 Paging p={};p.cr3=0x12345018;p.cr2=0xdeadbeef;
 p.base.page=p.cr3>>12;p.base.addr=p.cr3&~0xfffu;
 if(badbase)++p.base.page;
 std::memset(&p.tlb,0xa5,sizeof(p.tlb));p.links.used=~0u;
 for(unsigned i=0;i<272;++i)p.firstmb[i]=271-i;
 Faults f={}; f.used=faults;
 std::ostringstream s;
 put(s,p.cr3);put(s,p.cr2);put(s,p.base);put(s,p.tlb.read);
 put(s,p.tlb.write);put(s,p.tlb.phys_page);put(s,p.links);
 put(s,p.firstmb);put(s,enabled);put(s,f);return s.str();
}
int main() {
 const auto good=wire();
 // Every truncation must fail, including discarded host-cache spans.
 for(size_t n=0;n<good.size();++n) {
  std::istringstream s(good.substr(0,n));auto before=rebuilds;
  POD_Load_CPU_Paging(s);assert(s.fail() && rebuilds==before);
 }
 for(const auto &bad:{wire(2),wire(1,81),wire(1,0,true)}) {
  std::istringstream s(bad);auto before=rebuilds;
  POD_Load_CPU_Paging(s);assert(s.fail() && rebuilds==before);
 }
 for(unsigned field=0;field<4;++field) {
  auto bad=wire(1,1);Faults f={};f.used=1;
  if(field==0)f.entries[0].cs=0x10000;
  if(field==1)f.entries[0].page_addr=UINT32_MAX;
  if(field==2)f.entries[0].mpl=4;
  if(field==3)f.entries[0].eip=Bitu(UINT32_MAX)+1;
  if(field==3 && sizeof(Bitu)==4)continue;
  bad.replace(bad.size()-sizeof(f),sizeof(f),(const char*)&f,sizeof(f));
  std::istringstream s(bad);auto before=rebuilds;
  POD_Load_CPU_Paging(s);assert(s.fail() && rebuilds==before);
 }
 std::istringstream s(good);POD_Load_CPU_Paging(s);
 assert(s && rebuilds==1 && paging.cr3==0x12345018 && paging.enabled);
 assert(paging.cr2==0xdeadbeef && paging.firstmb[0]==271 && paging.firstmb[271]==0);
 assert(paging.tlb.read[0]==0 && paging.links.used==0);
 std::istringstream off(wire(0,80));POD_Load_CPU_Paging(off);
 assert(off && rebuilds==2 && !paging.enabled && pf_queue.used==80);
 std::ostringstream first;POD_Save_CPU_Paging(first);
 std::memset(&paging.tlb,0xab,sizeof(paging.tlb));paging.links.used=~0u;
 std::ostringstream second;POD_Save_CPU_Paging(second);
 assert(first.str()==second.str());
 const auto cache_begin=sizeof(paging.cr3)+sizeof(paging.cr2)+sizeof(paging.base);
 const auto cache_size=sizeof(paging.tlb.read)+sizeof(paging.tlb.write)+sizeof(paging.tlb.phys_page)+sizeof(paging.links);
 assert(first.str().substr(cache_begin,cache_size)==std::string(cache_size,'\0'));
}
'''
path = work / 'consumer.cpp'
path.write_text(source)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-g', str(path), '-o', str(work/'consumer')], check=True)
subprocess.run([str(work/'consumer')], check=True)
print('paging consumer: all truncations, bool/base/fault bounds, native cache rebuild passed')
