#!/usr/bin/env python3
"""MSCDEX actual sequential consumer, stub native audio."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mscdex-consumption';work.mkdir(exist_ok=True)
t=(root/'src/dos/dos_mscdex.cpp').read_text();codec=t[t.index('void POD_Load_DOS_Mscdex('):]
binding=t[t.index('void CMscdex::LoadState('):t.index('\nvoid POD_Save_DOS_Mscdex',t.index('void CMscdex::LoadState('))]
source=r'''
#include <cassert>
#include <cstdint>
#include <sstream>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
bool dos_kernel_disabled=false;
constexpr unsigned MSCDEX_MAX_DRIVES=8;
struct TMSF {uint8_t min,sec,fr;};
unsigned native_count=1,stops=0,plays=0,loads=0;
unsigned GetNumDrives(){return native_count;}
bool StopAudio(unsigned){++stops;return true;}
// Callback identity is tested by mscdex-callback-consumption.py.
struct CMscdex {uint16_t defaultBufSeg=0,rootDriverHeaderSeg=0;
 bool NativeCallbacksIntact(){return true;}void LoadState(std::istream&);};
'''+binding+r'''
struct Native {bool PlayAudioMSF(unsigned,uint32_t,uint32_t){++plays;return true;}
 void LoadState(std::istream&){++loads;}}native,*mscdex=&native;
'''+codec+r'''
std::string wire(uint16_t count,uint8_t playing=0,uint8_t second=0){
 std::ostringstream s;s.write((char*)&count,2);s.put(playing);s.put(0);
 for(int i=0;i<3;++i){s.put(0);s.put(second);s.put(0);}return s.str();}
bool load(const std::string& b){std::istringstream s(b);POD_Load_DOS_Mscdex(s);return bool(s);}
int main(){
 CMscdex service;
 for(uint16_t a: {uint16_t(0),uint16_t(1)})for(uint16_t b: {uint16_t(0),uint16_t(1)}){
  std::string bytes((char*)&a,2);bytes.append((char*)&b,2);
  std::istringstream s(bytes);service.LoadState(s);
  assert(bool(s)==(!a && !b));assert(!service.defaultBufSeg && !service.rootDriverHeaderSeg);
 }
 for(unsigned n=0;n<4;++n){std::istringstream s(std::string(n,'\0'));service.LoadState(s);assert(s.fail());}
 const auto good=wire(1);
 for(unsigned i=0;i<good.size();++i)assert(!load(good.substr(0,i)) && !stops && !plays && !loads);
 assert(!load(wire(0)) && !stops);assert(!load(wire(9)) && !stops);
 assert(!load(wire(1,2)) && !stops);assert(!load(wire(1,0,60)) && !stops);
 assert(!load(wire(1,1)) && !stops); // invalid active end lacks 150-frame bias
 assert(load(good) && stops==1 && !plays && loads==1);
}
'''
p=work/'consumer.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('MSCDEX inventory/truncation/audio bounds pre-effect cuts passed')
