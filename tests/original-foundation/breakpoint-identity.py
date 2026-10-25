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
 static bool DeleteById(uintptr_t);
 static uintptr_t ConsumeLastTriggered();
};
uintptr_t CBreakpoint::next_id=1,CBreakpoint::lastTriggered=0;
std::list<CBreakpoint*> CBreakpoint::BPoints;
'''
source+='\n'.join(body(x) for x in ['CBreakpoint::CBreakpoint(void)', 'bool CBreakpoint::DeleteBreakpoint(CBreakpoint* breakpoint)', 'bool CBreakpoint::DeleteById(uintptr_t id)', 'uintptr_t CBreakpoint::ConsumeLastTriggered(void)'])
source+=r'''
int main() {
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
