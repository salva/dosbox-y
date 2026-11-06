#!/usr/bin/env python3
"""Original RTC owner traversal + actual calendar tick, no host-clock replay.
Reduced codec harness, not full guest CMOS/IRQ or host-sync qualification.
"""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'cmos-calendar'
work.mkdir(exist_ok=True)
s=(root/'src/hardware/cmos.cpp').read_text()
state=s[s.index('static struct {'):s.index('const uint8_t BIOS_DATE_months')]
calendar=s[s.index('const uint8_t BIOS_DATE_months'):s.index('static void cmos_tick')]
tick=s[s.index('static void cmos_tick'):s.index('\n}',s.index('static void cmos_tick'))+2]
owner=s[s.index('class SerializeCmos'):s.index('} dummy;',s.index('class SerializeCmos'))+1]+';'
source=r'''
#include <cassert>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <utility>
#define LOG(...) noop
void noop(const char*,...){}
bool sync_time=false;
struct {struct{unsigned year,month,day;}date;}dos;
struct SerializeGlobalPOD {
 std::vector<std::pair<void*,size_t>> spans;
 SerializeGlobalPOD(const char*){}
 template<class T>void registerPOD(T &v){spans.push_back({&v,sizeof(v)});}
 std::vector<char> save(){std::vector<char>b;for(auto p:spans){auto c=(char*)p.first;b.insert(b.end(),c,c+p.second);}return b;}
 void load(const std::vector<char>&b){size_t n=0;for(auto p:spans){assert(n+p.second<=b.size());memcpy(p.first,b.data()+n,p.second);n+=p.second;}assert(n==b.size());}
};
'''+state+calendar+tick+owner+r'''
int main(){
 SerializeCmos owner;
 cmos.clock={59,59,23,7,28,2,2024}; cmos.alarm={12,34,5};
 cmos.bcd=false;cmos.ampm=true;cmos.lock=true;
 cmos.clock_time_t=123456;cmos_sync_flag=true;
 cmos_sync_sec=11;cmos_sync_min=22;cmos_sync_hour=3;
 auto saved=owner.save();
 cmos={};cmos_sync_flag=false;cmos_sync_sec=cmos_sync_min=cmos_sync_hour=0;
 owner.load(saved);
 assert(owner.save()==saved); // includes clock/mode/alarm and pending BIOS handoff
 assert(cmos.bcd==false && cmos.ampm && cmos.lock);
 assert(cmos.alarm.sec==12 && cmos.alarm.min==34 && cmos.alarm.hour==5);
 assert(cmos.clock_time_t==123456 && cmos_sync_flag && cmos_sync_hour==3);
 // Explicit continuation only: leap day/weekday and subsequent month rollover.
 cmos_tick();assert(cmos.clock.day==29 && cmos.clock.month==2 && cmos.clock.weekday==1);
 assert(cmos.clock.hour==0 && cmos.clock.min==0 && cmos.clock.sec==0);
 assert(cmos.clock_time_t==123457);
 cmos.clock={59,59,23,1,29,2,2024};cmos_tick();
 assert(cmos.clock.day==1 && cmos.clock.month==3 && cmos.clock.year==2024);
}
'''
p=work/'consumer.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',str(p),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Actual RTC original traversal calendar/alarm/modes/pending sync roundtrip -> explicit leap-day/month tick ASan/UBSan PASS; reduced codec not IRQ/host-sync proof')
