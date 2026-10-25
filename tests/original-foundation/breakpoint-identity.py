#!/usr/bin/env python3
"""Exercise real breakpoint identity/lifetime bodies with inert owner effects."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'breakpoint-identity'
work.mkdir(exist_ok=True)
text=(root/'src/debug/debug.cpp').read_text()
def body(start):
    at=text.index(start); begin=text.index('{',at); depth=1; end=begin+1
    while depth:
        depth += (text[end]=='{')-(text[end]=='}'); end+=1
    return text[at:end]
source=r'''
#include <cstdint>
#include <list>
#include <cassert>
#include <stdexcept>
#define C_HEAVY_DEBUG 1
#define BKPNT_UNKNOWN 0
#define BKPNT_INTERRUPT 1
#define BPINT_ALL 0x100
using PhysPt=uint32_t;
void E_Exit(const char*) { throw std::runtime_error("fatal"); }
class CBreakpoint {
public:
 int type,location,segment,offset,intNr,ahValue,alValue; bool active,once;
 uintptr_t id;
 static uintptr_t next_id,lastTriggered;
 static std::list<CBreakpoint*> BPoints;
 CBreakpoint();
 uintptr_t GetId() const {return id;}
 void Activate(bool value) {active=value;}
 static bool DeleteBreakpoint(CBreakpoint*);
 static bool DeleteBreakpoint(uint16_t,uint32_t);
 static CBreakpoint* FindPhysBreakpoint(uint16_t,uint32_t,bool);
 static bool CheckIntBreakpoint(uint32_t,uint8_t,uint16_t,uint16_t);
 static bool DeleteByIndex(uint16_t);
 static void DeleteAll();
 int GetType() const {return type;}
 bool IsActive() const {return active;}
 uint8_t GetIntNr() const {return intNr;}
 uint16_t GetValue() const {return ahValue;}
 uint16_t GetOther() const {return alValue;}
 bool GetOnce() const {return once;}
 static bool DeleteById(uintptr_t);
 static uintptr_t ConsumeLastTriggered();
};
uintptr_t CBreakpoint::next_id=1,CBreakpoint::lastTriggered=0;
std::list<CBreakpoint*> CBreakpoint::BPoints;
'''
source+='\n'.join(body(x) for x in ['CBreakpoint::CBreakpoint(void)', 'bool CBreakpoint::DeleteBreakpoint(CBreakpoint* breakpoint)', 'bool CBreakpoint::DeleteById(uintptr_t id)', 'uintptr_t CBreakpoint::ConsumeLastTriggered(void)', 'bool CBreakpoint::CheckIntBreakpoint(PhysPt adr', 'bool CBreakpoint::DeleteByIndex(uint16_t index)', 'void CBreakpoint::DeleteAll()', 'bool CBreakpoint::DeleteBreakpoint(uint16_t seg, uint32_t off)'])
source+=r'''
CBreakpoint* CBreakpoint::FindPhysBreakpoint(uint16_t seg,uint32_t off,bool once) {
 for(auto bp:BPoints)if(bp->segment==seg && bp->offset==int(off) && bp->once==once)return bp;
 return nullptr;
}
int main() {
 // Real interrupt matching and one-shot retirement preserve a value receipt,
 // including wildcard AH/AL and misses; Activate is an inert owner double.
 for(bool once:{false,true}) {
  auto *bp=new CBreakpoint; auto id=bp->GetId();
  bp->type=BKPNT_INTERRUPT;bp->intNr=0x21;bp->ahValue=0x4c;
  bp->alValue=BPINT_ALL;bp->once=once;bp->Activate(true);
  CBreakpoint::BPoints.push_back(bp);
  assert(!CBreakpoint::CheckIntBreakpoint(0,0x20,0x4c,0));
  assert(!CBreakpoint::CheckIntBreakpoint(0,0x21,0x3d,0));
  assert(!CBreakpoint::ConsumeLastTriggered());
  assert(CBreakpoint::CheckIntBreakpoint(0,0x21,0x4c,5));
  assert(CBreakpoint::ConsumeLastTriggered()==id);
  assert(!CBreakpoint::ConsumeLastTriggered());
  assert(once? !CBreakpoint::DeleteById(id):CBreakpoint::DeleteById(id));
 }
 // All explicit removal routes invalidate a retained receipt.
 auto *removed=new CBreakpoint;auto removed_id=removed->GetId();
 removed->segment=12;removed->offset=34;
 CBreakpoint::BPoints.push_back(removed);CBreakpoint::lastTriggered=removed_id;
 assert(CBreakpoint::DeleteBreakpoint(12,34));
 assert(!CBreakpoint::ConsumeLastTriggered());
 removed=new CBreakpoint;CBreakpoint::BPoints.push_back(removed);
 CBreakpoint::lastTriggered=removed->GetId();assert(CBreakpoint::DeleteByIndex(0));
 assert(!CBreakpoint::ConsumeLastTriggered());
 removed=new CBreakpoint;CBreakpoint::BPoints.push_back(removed);
 CBreakpoint::lastTriggered=removed->GetId();CBreakpoint::DeleteAll();
 assert(!CBreakpoint::ConsumeLastTriggered() && CBreakpoint::BPoints.empty());
 auto *first=new CBreakpoint; auto old=first->GetId();
 CBreakpoint::BPoints.push_back(first); CBreakpoint::lastTriggered=old;
 assert(CBreakpoint::DeleteById(old));
 assert(!CBreakpoint::ConsumeLastTriggered());
 for(unsigned n=0;n<10000;++n) {
  auto *next=new CBreakpoint; auto id=next->GetId();
  CBreakpoint::BPoints.push_back(next);
  assert(id>old && !CBreakpoint::DeleteById(old));
  CBreakpoint::lastTriggered=id;
  assert(CBreakpoint::ConsumeLastTriggered()==id);
  assert(!CBreakpoint::ConsumeLastTriggered());
  assert(CBreakpoint::DeleteById(id)); old=id;
 }
 assert(!CBreakpoint::DeleteById(0));
 CBreakpoint::next_id=UINTPTR_MAX;
 bool fatal=false;try {CBreakpoint limit;}catch(const std::runtime_error&){fatal=true;}
 assert(fatal && CBreakpoint::BPoints.empty());
}
'''
(work/'identity.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'identity.cpp'),'-o',str(work/'identity')],check=True)
subprocess.run([str(work/'identity')],check=True)
print('actual breakpoint identity/deletion bodies PASS; not session/transport qualification')
