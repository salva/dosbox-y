#!/usr/bin/env python3
"""Reduced-layout production EMS consumed-state checks, not startup proof."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'ems-consumption'
work.mkdir(exist_ok=True)
h = (root/'include/dosbox.h').read_text()
pod = h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
x = (root/'src/ints/ems.cpp').read_text()
native=x[x.index('    bool NativeBindings('):x.index('\n\tRealPt Get_EMS_vm86control')]
codec = x[x.index('namespace\n{\nclass SerializeEMS'):].rsplit('#endif', 1)[0]
source = r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
using Bitu=uintptr_t;using MemHandle=int32_t;using PhysPt=uint32_t;
constexpr unsigned CB_IRET=1,CB_RETF=2,CB_MAX=128;
using CallBack_Handler=Bitu(*)(void);
Bitu INT4B_Handler(){return 1;}Bitu INT67_Handler(){return 2;}
Bitu VCPI_PM_Handler(){return 3;}Bitu WinVM86Ctl(){return 4;}Bitu V86_Monitor(){return 5;}
CallBack_Handler CallBack_Handlers[CB_MAX]={};
uint8_t ram[0x100000]={};
PhysPt PhysMake(uint16_t s,uint16_t o){return s*16+o;}
PhysPt Real2Phys(uint32_t p){return PhysMake(p>>16,p&65535);}
uint8_t mem_readb(PhysPt a){assert(a<sizeof ram);return ram[a];}
uint16_t mem_readw(PhysPt a){return mem_readb(a)|(mem_readb(a+1)<<8);}
unsigned callback_location_delta=0;
struct Callback {unsigned slot;unsigned Get_callback(){return slot;}uint32_t Get_RealPointer(){return 0xf0000000+slot*32+callback_location_delta;}};
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+r'''
constexpr uint16_t NULL_HANDLE=0xffff,NULL_PAGE=0xffff;
constexpr unsigned EMM_MAX_HANDLES=200,EMM_MAX_PHYS=4;
struct EMM_Mapping {uint16_t handle=NULL_HANDLE,page=NULL_PAGE;};
struct EMM_Handle {uint16_t pages=NULL_HANDLE;MemHandle mem=0;char name[8]={};bool saved_page_map=false;EMM_Mapping page_map[4];};
EMM_Handle emm_handles[200];EMM_Mapping emm_mappings[4],emm_segmentmappings[64];
Bitu ems_type=1,EMM_PAGEFRAME=0xe000,call_int67=12;uint16_t GEMMIS_seg=0;
struct {bool enabled=false;uint16_t ems_handle=0;uint32_t pm_interface=0,private_area=0;uint8_t pic1_remapping=0,pic2_remapping=0;} vcpi;
bool EMS_Active(){return ems_type!=0;}Bitu MEM_TotalPages(){return 4096;}
Bitu MEM_AllocatedPages(MemHandle h){assert(h>0 && h<4096);return h==256?4:0;}
struct EMS {uint16_t ems_baseseg=0xc000;unsigned oshandle_memsize_16kb=24;
Callback call_vdma{11},call_vcpi{0},call_win_vm86_ctl{0},call_v86mon{0};
'''+native+r'''} ems;EMS *test=&ems;
'''+codec+r'''
std::string save(){std::ostringstream s;SaveState::instance().owner->getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);SaveState::instance().owner->setBytes(s);return bool(s);}
int main(){
 const auto init=[](PhysPt a,unsigned slot){ram[a]=0xfe;ram[a+1]=0x38;ram[a+2]=slot;ram[a+4]=0xcf;};
 const auto vdma=Real2Phys(ems.call_vdma.Get_RealPointer()),int67=PhysMake(ems.ems_baseseg,4);
 init(vdma,11);init(int67,12);
 CallBack_Handlers[11]=INT4B_Handler;CallBack_Handlers[12]=INT67_Handler;
 CallBack_Handlers[13]=VCPI_PM_Handler;CallBack_Handlers[14]=WinVM86Ctl;
 CallBack_Handlers[15]=V86_Monitor;
 emm_handles[1].pages=1;emm_handles[1].mem=256;emm_mappings[0]={1,0};
 const auto bytes=save();assert(load(bytes));
 for(auto slot:{11u,12u}) {
  auto handler=CallBack_Handlers[slot];CallBack_Handlers[slot]=nullptr;
  assert(!load(bytes));std::ostringstream absent;SaveState::instance().owner->getBytes(absent);assert(!absent);
  CallBack_Handlers[slot]=(slot==11?INT67_Handler:INT4B_Handler);
  assert(!load(bytes));std::ostringstream wrong;SaveState::instance().owner->getBytes(wrong);assert(!wrong);
  CallBack_Handlers[slot]=handler;assert(load(bytes));
 }
 // Same role/slot/type and valid copied bodies at a different native location
 // are not the same binding. Exercise location independently of body checks.
 callback_location_delta=0x1000;
 init(Real2Phys(ems.call_vdma.Get_RealPointer()),11);
 assert(!load(bytes));callback_location_delta=0;assert(load(bytes));
 for(auto a:{vdma,int67}) for(unsigned i=0;i<5;++i){ram[a+i]^=1;assert(!load(bytes));ram[a+i]^=1;}
 for(size_t i=0;i<bytes.size();++i)assert(!load(bytes.substr(0,i)));
 for(size_t i=0;i<100;++i){auto b=bytes;b[i]^=1;assert(!load(b));}
 assert(load(bytes));emm_handles[1].mem=-1;assert(!load(save()));
 assert(load(bytes));emm_handles[1].pages=2;assert(!load(save()));
 assert(load(bytes));emm_mappings[0]={200,0};assert(!load(save()));
 assert(load(bytes));emm_mappings[0]={1,1};assert(!load(save()));
 assert(load(bytes));emm_segmentmappings[0]={NULL_HANDLE,0};assert(!load(save()));
 assert(load(bytes));emm_handles[1].saved_page_map=true;emm_handles[1].page_map[0]={1,1};assert(!load(save()));
 assert(load(bytes));GEMMIS_seg=123;const auto lazy=save();GEMMIS_seg=0;assert(!load(lazy));
 GEMMIS_seg=0;assert(load(bytes));
 vcpi.pm_interface=123;const auto wrong_interface=save();vcpi.pm_interface=0;
 assert(!load(wrong_interface));vcpi.pm_interface=0;assert(load(bytes));
 vcpi.private_area=456;const auto wrong_area=save();vcpi.private_area=0;
 assert(!load(wrong_area));vcpi.private_area=0;assert(load(bytes));
 vcpi.enabled=true;{std::ostringstream s;SaveState::instance().owner->getBytes(s);assert(!s);}
 vcpi.enabled=false;assert(load(bytes));
 auto b=bytes;b[100+2+sizeof(MemHandle)+8]=2;assert(!load(b));
 assert(load(bytes));
 // Actual PM/control/monitor bodies, including the private-area IRETD copy.
 ems.call_vcpi.slot=13;ems.call_win_vm86_ctl.slot=14;ems.call_v86mon.slot=15;
 vcpi.enabled=true;vcpi.private_area=0x20000;
 for(auto c:{ems.call_vcpi,ems.call_win_vm86_ctl,ems.call_v86mon}){
  auto a=Real2Phys(c.Get_RealPointer());init(a,c.slot);ram[a+4]=c.slot==15?0xcf:0xcb;
 }
 const auto monitor=vcpi.private_area+0x2e00;
 init(monitor,15);ram[monitor+4]=0x66;ram[monitor+5]=0xcf;
 const auto enabled=save();assert(load(enabled));
 for(auto slot:{13u,14u,15u}) {
  auto handler=CallBack_Handlers[slot];
  for(auto bad:{static_cast<CallBack_Handler>(nullptr),INT67_Handler}) {
   CallBack_Handlers[slot]=bad;assert(!load(enabled));
   std::ostringstream out;SaveState::instance().owner->getBytes(out);assert(!out);
  }
  CallBack_Handlers[slot]=handler;assert(load(enabled));
 }
 for(auto a:{Real2Phys(ems.call_vcpi.Get_RealPointer()),Real2Phys(ems.call_win_vm86_ctl.Get_RealPointer()),Real2Phys(ems.call_v86mon.Get_RealPointer())})
  for(unsigned i=0;i<5;++i){ram[a+i]^=1;assert(!load(enabled));ram[a+i]^=1;}
 for(unsigned i=0;i<6;++i){ram[monitor+i]^=1;assert(!load(enabled));ram[monitor+i]^=1;}
 assert(load(enabled));
 // IVT hook ownership is guest state, not an EMS native-binding equality.
 ram[0x67*4]=0x42;ram[0x4b*4]=0x19;assert(load(enabled));
}
'''
p = work/'consumer.cpp'
p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('EMS codec: binding, truncation, bool, page chains, mappings and lazy GEMMIS refusal passed')
