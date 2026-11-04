#!/usr/bin/env python3
"""Actual shell owner codec in reduced memory, not fresh-process qualification."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'shell-bindings'
work.mkdir(exist_ok=True)
h=(root/'include/dosbox.h').read_text()
pod=h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
t=(root/'src/shell/shell.cpp').read_text()
codec=t[t.index('namespace {\nclass SerializeShellBindings'):t.index('} shell_binding_codec;')+len('} shell_binding_codec;\n}')]
source=r'''
#include <cassert>
#include <sstream>
#include <string>
#include <cstdint>
#include <cstring>
#include <vector>
#include <initializer_list>
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 static SaveState& instance(){static SaveState s;return s;} void registerComponent(const std::string&,Component&) {}};
'''+pod+r'''
using Bitu=uint32_t;using PhysPt=uint32_t;
constexpr uint32_t CB_MAX=128,CB_IRET=1,CB_RETF=2,CB_IRET_STI=3;
uint16_t shell_psp=0x700;Bitu call_shellstop=1,call_int23=2,call_int2e=3;
using Handler=Bitu(*)(void);
static Bitu shellstop_handler(){return 1;}
static Bitu INT23_Handler(){return 2;}
static Bitu INT2E_Handler(){return 3;}
Handler CallBack_Handlers[CB_MAX]={nullptr,shellstop_handler,INT23_Handler,INT2E_Handler};
uint8_t ram[0x100000]={};
PhysPt PhysMake(uint16_t s,uint16_t o){return s*16u+o;}
PhysPt CALLBACK_PhysPointer(Bitu s){return 0xf0000+s*32;}
uint8_t mem_readb(PhysPt a){assert(a<sizeof ram);return ram[a];}
uint16_t mem_readw(PhysPt a){return mem_readb(a)|(mem_readb(a+1)<<8);}
void emit(PhysPt a,Bitu slot,bool sti,uint8_t ret){if(sti)ram[a++]=0xfb;
 ram[a++]=0xfe;ram[a++]=0x38;ram[a++]=slot;ram[a++]=slot>>8;ram[a]=ret;}
'''+codec+r'''
std::string save(){std::ostringstream s;shell_binding_codec.getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);shell_binding_codec.setBytes(s);return bool(s);}
int main(){
 const auto stop=CALLBACK_PhysPointer(call_shellstop),i23=PhysMake(shell_psp,296),i2e=PhysMake(shell_psp,280);
 emit(stop,call_shellstop,false,0xcf);emit(i23,call_int23,false,0xcb);emit(i2e,call_int2e,true,0xcf);
 const auto bytes=save();assert(bytes.size()==52 && load(bytes));
 for(auto slot:{call_shellstop,call_int23,call_int2e}) {
  auto handler=CallBack_Handlers[slot];CallBack_Handlers[slot]=nullptr;
  assert(!load(bytes));std::ostringstream s;shell_binding_codec.getBytes(s);assert(!s);
  CallBack_Handlers[slot]=(slot==call_shellstop?INT23_Handler:shellstop_handler);
  assert(!load(bytes));std::ostringstream wrong;shell_binding_codec.getBytes(wrong);assert(!wrong);
  CallBack_Handlers[slot]=handler;assert(load(bytes));
 }
 for(size_t n=0;n<bytes.size();++n){assert(!load(bytes.substr(0,n)));auto b=bytes;b[n]^=1;assert(!load(b));}
 for(auto address:{stop,i23,i2e})for(unsigned n=0;n<(address==i2e?6u:5u);++n){
  ram[address+n]^=1;assert(!load(bytes));std::ostringstream s;shell_binding_codec.getBytes(s);assert(!s);ram[address+n]^=1;}
 // Hooks are intentionally not part of native identity/body comparison.
 memset(ram+0x23*4,0xa5,4);memset(ram+0x2e*4,0x5a,4);assert(load(bytes));
 ++call_int23;assert(!load(bytes));--call_int23;assert(load(bytes));
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Shell bindings: every field/truncation/body-byte mismatch refuses; IVT hooks preserved')
