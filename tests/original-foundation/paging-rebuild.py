#!/usr/bin/env python3
"""Actual paging loader/reset: hostile cached indices must never be walked."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'paging-rebuild'
work.mkdir(exist_ok=True)
t=(root/'src/cpu/paging.cpp').read_text()
a=t.index('void PAGING_InitTLB(void)'); b=t.index('void PAGING_ClearTLB',a)
init=t[a:b]
a=t.index('void POD_Load_CPU_Paging('); b=t.index('uint8_t PageHandler_HostPtReadB',a)
load=t[a:b]
s='''
#include <cassert>
#include <cstdint>
#include <sstream>
#include <limits>
using Bitu=unsigned;
constexpr unsigned TLB_SIZE=32,PAGING_LINKS=8,PF_QUEUESIZE=4;
int init_page_handler;
struct Links {unsigned used;uint32_t entries[PAGING_LINKS];};
struct Paging {unsigned cr3,cr2,base;struct {
 void *read[TLB_SIZE],*write[TLB_SIZE];int *readhandler[TLB_SIZE],*writehandler[TLB_SIZE];
 unsigned phys_page[TLB_SIZE];} tlb;
 Links links,ur_links,krw_links,kr_links; unsigned firstmb[4];bool enabled;
} paging;
struct Queue {unsigned used;unsigned entries[PF_QUEUESIZE];} pf_queue;
[[noreturn]] void E_Exit(const char*){throw 7;}
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
'''+init+load+'''
#define PUT(v) out.write(reinterpret_cast<const char*>(&v),sizeof(v))
std::string archive(unsigned used){
 std::ostringstream out;PUT(paging.cr3);PUT(paging.cr2);PUT(paging.base);
 PUT(paging.tlb.read);PUT(paging.tlb.write);PUT(paging.tlb.phys_page);
 PUT(paging.links);PUT(paging.firstmb);PUT(paging.enabled);
 pf_queue.used=used;PUT(pf_queue);return out.str();
}
int main(){
 for(auto &n:paging.links.entries)n=UINT32_MAX;
 paging.links.used=UINT32_MAX;
 auto bytes=archive(0);
 std::istringstream in(bytes);POD_Load_CPU_Paging(in);
 assert(in && !paging.links.used && !paging.ur_links.used && !paging.krw_links.used && !paging.kr_links.used);
 for(unsigned i=0;i<TLB_SIZE;++i){assert(!paging.tlb.read[i] && !paging.tlb.write[i]);
 assert(paging.tlb.readhandler[i]==&init_page_handler && paging.tlb.writehandler[i]==&init_page_handler);}
 for(unsigned n=0;n<bytes.size();++n){
  std::istringstream shortin(bytes.substr(0,n));bool fatal=false;
  try{POD_Load_CPU_Paging(shortin);}catch(int x){fatal=x==7;}assert(fatal);
 }
 std::istringstream bad(archive(PF_QUEUESIZE+1));bool fatal=false;
 try{POD_Load_CPU_Paging(bad);}catch(int x){fatal=x==7;}assert(fatal);
}
'''
(work/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('paging actual loader/cache reset ASan/UBSan PASS; no semantic/frontier qualification')
