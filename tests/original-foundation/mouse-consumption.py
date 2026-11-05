#!/usr/bin/env python3
"""Actual mouse struct/codec with reduced base and PC98 transport doubles."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mouse-consumption';work.mkdir(exist_ok=True)
t=(root/'src/ints/mouse.cpp').read_text()
state=t[t.index('static struct {\n    uint8_t buttons;'):t.index('} mouse;')+len('} mouse;')]
codec=t[t.index('class SerializeMouse :'):t.index('} dummy;',t.index('class SerializeMouse :'))+len('} dummy;')]
roles=t[t.index('struct MouseNativeRole {'):t.index('void MOUSE_Unsetup_DOS(void)')]
source=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>
#include <math.h>
#include <limits>
#include <cstddef>
using pic_tickindex_t=double;
#define MOUSE_BUTTONS 3
#define QUEUE_SIZE 32
#define CURSORX 16
#define CURSORY 16
struct button_event {uint8_t type,buttons;};
uint16_t defaultScreenMask[16],defaultCursorMask[16],defaultCursorMaskNEC[32],userdefScreenMask[16],userdefCursorMask[32];
uint16_t ps2cbseg=0,ps2cbofs=0;
bool useps2callback=false,ps2callbackinit=false;
uint8_t aux_pkt[4]={},aux_pkt_len=0,gfxReg3CE[9]={},index3C4=0,gfxReg3C5[5]={};
const std::vector<uint32_t> UNLOCK_SEQ_INTELLIMOUSE={6,5,4};
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
struct SerializeGlobalPOD {
 SerializeGlobalPOD(const char*){}
 virtual void getBytes(std::ostream&){}
 virtual void setBytes(std::istream&){}
};
void PC98_Mouse_SaveState(std::ostream&){}
void PC98_Mouse_LoadState(std::istream&){}
using Bitu=uint32_t;using PhysPt=uint32_t;using CallBack_Handler=Bitu(*)();
constexpr Bitu CB_MAX=128;
Bitu call_int33=0,call_mouse_bd=0,call_int74=0,int74_ret_callback=0,call_ps2=0,call_uir=0;
CallBack_Handler CallBack_Handlers[CB_MAX]={};
uint8_t ram[4096]={};
uint8_t phys_readb(PhysPt a){assert(a<sizeof ram);return ram[a];}
PhysPt CALLBACK_PhysPointer(Bitu s){return s*32;}
Bitu CALLBACK_Setup(Bitu s,CallBack_Handler h,Bitu,PhysPt a,const char*){CallBack_Handlers[s]=h;ram[a]=uint8_t(s);return 1;}
Bitu handler(){return 0;}
'''+roles+'\n'+state+'\n'+codec+r'''
std::string save(){std::ostringstream out;static_cast<SerializeGlobalPOD&>(dummy).getBytes(out);assert(out);return out.str();}
bool load(const std::string&s){std::istringstream in(s);static_cast<SerializeGlobalPOD&>(dummy).setBytes(in);return bool(in);}
int main(){
 Bitu *slots[]={&call_int33,&call_mouse_bd,&call_int74,&int74_ret_callback,&call_ps2,&call_uir};
 for(unsigned role=0;role<6;++role){*slots[role]=role+1;MouseSetupRole(role,*slots[role],handler,1,"fixture");}
 mouse.screenMask=defaultScreenMask;mouse.cursorMask=defaultCursorMask;
 auto valid=save();assert(load(valid));assert(mouse.screenMask==defaultScreenMask && mouse.cursorMask==defaultCursorMask);
 for(unsigned role=0;role<6;++role){
  auto &native=mouse_native_roles[role];
  CallBack_Handlers[native.slot]=nullptr;assert(!load(valid));CallBack_Handlers[native.slot]=handler;
  ram[native.address]^=1;assert(!load(valid));ram[native.address]^=1;
  ++*slots[role];assert(!load(valid));--*slots[role];assert(load(valid));
 }
 for(auto cursor:{defaultCursorMask,userdefCursorMask,defaultCursorMaskNEC}) {
  mouse.screenMask=userdefScreenMask;mouse.cursorMask=cursor;auto s=save();
  assert(load(s));assert(mouse.screenMask==userdefScreenMask && mouse.cursorMask==cursor);
 }
 for(size_t n=0;n<valid.size();++n) assert(!load(valid.substr(0,n)));
 assert(load(valid));
 const size_t mouse_offset=2*sizeof(uint16_t)+2*sizeof(bool)+sizeof(aux_pkt)+sizeof(aux_pkt_len)+sizeof(userdefScreenMask)+sizeof(userdefCursorMask);
 for(size_t offset:{offsetof(decltype(mouse),background),offsetof(decltype(mouse),enabled),offsetof(decltype(mouse),inhibit_draw),offsetof(decltype(mouse),timer_in_progress),offsetof(decltype(mouse),first_range_setx),offsetof(decltype(mouse),first_range_sety),offsetof(decltype(mouse),in_UIR),offsetof(decltype(mouse),polled)}) {
  auto bad=valid;bad[mouse_offset+offset]=char(2);assert(!load(bad));
 }
 for(size_t offset:{size_t(4),size_t(5)}){auto bad=valid;bad[offset]=char(2);assert(!load(bad));}
 const size_t float_fields[]={offsetof(decltype(mouse),add_x),offsetof(decltype(mouse),add_y),
  offsetof(decltype(mouse),mickey_accum_x),offsetof(decltype(mouse),mickey_accum_y),
  offsetof(decltype(mouse),x),offsetof(decltype(mouse),y),
  offsetof(decltype(mouse),ps2x),offsetof(decltype(mouse),ps2y),
  offsetof(decltype(mouse),mickeysPerPixel_x),offsetof(decltype(mouse),mickeysPerPixel_y),
  offsetof(decltype(mouse),pixelPerMickey_x),offsetof(decltype(mouse),pixelPerMickey_y),
  offsetof(decltype(mouse),senv_x),offsetof(decltype(mouse),senv_y)};
 for(size_t offset:float_fields) {
  for(float value:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
   auto bad=valid;memcpy(&bad[mouse_offset+offset],&value,sizeof(value));assert(!load(bad));
  }
 }
 for(pic_tickindex_t value:{std::numeric_limits<pic_tickindex_t>::infinity(),
     -std::numeric_limits<pic_tickindex_t>::infinity(),std::numeric_limits<pic_tickindex_t>::quiet_NaN()}) {
  auto bad=valid;memcpy(&bad[mouse_offset+offsetof(decltype(mouse),hidden_at)],&value,sizeof(value));assert(!load(bad));
 }
 for(unsigned which=0;which<2;++which){auto bad=valid;bad[bad.size()-1-which]=char(255);assert(!load(bad));assert(!mouse.screenMask && !mouse.cursorMask);}
 assert(load(valid));mouse.events=QUEUE_SIZE+1;auto bad=save();assert(!load(bad));
 assert(load(valid));aux_pkt_len=5;bad=save();assert(!load(bad));
 assert(load(valid));mouse.ps2_unlock_idx=3;bad=save();assert(!load(bad));
 assert(load(valid));mouse.screenMask=nullptr;std::ostringstream out;static_cast<SerializeGlobalPOD&>(dummy).getBytes(out);assert(out.fail());
}
'''
# Exercise both pic.h ABI choices rather than silently equating the configured
# x86 long-double tick representation with double.
for tick_type in ('double','long double'):
 p=work/('consumer-'+tick_type.replace(' ','-')+'.cpp')
 p.write_text(source.replace('using pic_tickindex_t=double;',f'using pic_tickindex_t={tick_type};'))
 binary=p.with_suffix('')
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
print('Mouse actual struct/codec: mask roles incl NEC, every truncation, invalid bool representations/nonfinite scalars/roles/events/AUX/unlock bounds passed')
