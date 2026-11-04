#!/usr/bin/env python3
"""Production XMS codec, reduced resource layout; not fresh-startup evidence."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'xms-consumption'
work.mkdir(exist_ok=True)
h=(root/'include/dosbox.h').read_text()
pod=h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
x=(root/'src/ints/xms.cpp').read_text()
codec=x[x.index('namespace\n{\nclass SerializeXMS'):]
source=r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
using Bitu=uintptr_t;using MemHandle=int32_t;using PhysPt=uint32_t;
constexpr uint32_t CB_MAX=128,CB_HOOKABLE=9;
using Handler=Bitu(*)(void);Bitu XMS_Handler(){return 0;}
Bitu wrong_handler(){return 1;}
Handler CallBack_Handlers[CB_MAX]={};
uint8_t ram[0x100000]={};
PhysPt Real2Phys(uint32_t p){return (p>>16)*16+(p&65535);}
uint8_t mem_readb(PhysPt a){assert(a<sizeof ram);return ram[a];}
uint16_t mem_readw(PhysPt a){return mem_readb(a)|(mem_readb(a+1)<<8);}
struct NativeXMS {Bitu NativeCallbackSlot()const{return 17;}} native_xms;
NativeXMS *test=&native_xms;
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+r'''
struct XMS_Block {Bitu size=0;MemHandle mem=-1;uint8_t locked=0;bool free=true;};
XMS_Block xms_handles[256];unsigned int XMS_HANDLES=50;uint32_t xms_callback=0xc0000010;
bool active=true;bool XMS_Active(){return active;}Bitu MEM_TotalPages(){return 4096;}
bool fragmented=false;
MemHandle MEM_NextHandle(MemHandle h){assert(h>0 && h<4096);return h==256?(fragmented?300:257):-1;}
'''+codec+r'''
std::string save(){std::ostringstream s;SaveState::instance().owner->getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);SaveState::instance().owner->setBytes(s);return bool(s);}
int main(){
 const auto body=Real2Phys(xms_callback)+5;
 ram[body]=0xfe;ram[body+1]=0x38;ram[body+2]=17;ram[body+4]=0xcb;
 CallBack_Handlers[17]=XMS_Handler;
 xms_handles[1]={8,256,1,false};const auto bytes=save();assert(load(bytes));
 for(auto handler:{wrong_handler,static_cast<Handler>(nullptr)}) {
  CallBack_Handlers[17]=handler;assert(!load(bytes));
  std::ostringstream out;SaveState::instance().owner->getBytes(out);assert(!out);
 }
 CallBack_Handlers[17]=XMS_Handler;assert(load(bytes));
 for(unsigned i=0;i<5;++i){ram[body+i]^=1;assert(!load(bytes));ram[body+i]^=1;}
 memset(ram+Real2Phys(xms_callback),0xcc,5);assert(load(bytes));
 for(size_t i=0;i<bytes.size();++i)assert(!load(bytes.substr(0,i)));
 for(size_t i=0;i<24;++i){auto b=bytes;b[i]^=1;assert(!load(b));}
 for(size_t i=24+sizeof(Bitu)+sizeof(MemHandle)+1;i<bytes.size();i+=sizeof(Bitu)+sizeof(MemHandle)+2){auto b=bytes;b[i]=2;assert(!load(b));}
 assert(load(bytes));xms_handles[1].size=9;assert(!load(save()));
 assert(load(bytes));xms_handles[1].mem=-1;assert(!load(save()));
 assert(load(bytes));xms_handles[1].size=UINT32_MAX;assert(!load(save()));
 assert(load(bytes));xms_handles[1]={0,0,0,false};assert(load(save()));
 assert(load(bytes));fragmented=true;assert(!load(bytes));fragmented=false;
 assert(load(bytes));xms_handles[2]=xms_handles[1];assert(!load(save()));assert(load(bytes));
 xms_handles[1]={0,4096,0,false};assert(!load(save()));
}
'''
p=work/'consumer.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('XMS codec: binding, truncation, bool, arithmetic and handle bounds passed')
