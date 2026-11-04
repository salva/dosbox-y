#!/usr/bin/env python3
"""Actual INT10 codec/type, reduced runtime: binding checks, not ROM integration."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'int10-consumption'
work.mkdir(exist_ok=True)
h=(root/'include/dosbox.h').read_text()
pod=h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
t=(root/'src/ints/int10.h').read_text()
layout=t[t.index('typedef struct {\n\tstruct {'):t.index('} Int10Data;')+len('} Int10Data;')]
t=(root/'src/ints/int10.cpp').read_text()
codec=t[t.index('class SerializeInt10 :'):t.index('} dummy;',t.index('class SerializeInt10 :'))+len('} dummy;')]
source=r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
using RealPt=uint32_t;using Bitu=uintptr_t;
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+layout+r'''
Int10Data int10={};Bitu call_10=12;
'''+codec+r'''
std::string save(){std::ostringstream s;dummy.getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);dummy.setBytes(s);return bool(s);}
int main(){int10.rom.font_8_first=0xc0000123;int10.rom.used=512;int10.vesa_nolfb=true;
 int10.vesa_setmode=0x118;int10.text_row=24;const auto bytes=save();assert(bytes.size()==30*4+3);
 int10.vesa_setmode=3;int10.text_row=0;assert(load(bytes));assert(int10.vesa_setmode==0x118 && int10.text_row==24);
 for(size_t n=0;n<bytes.size();++n)assert(!load(bytes.substr(0,n)));
 for(unsigned n=0;n<30;++n){auto bad=bytes;bad[n*4]^=1;assert(!load(bad));}
 assert(call_10==12 && int10.rom.font_8_first==0xc0000123 && int10.vesa_nolfb);
 call_10=13;assert(!load(bytes));call_10=12;assert(load(bytes));
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('INT10: native locations/variants retained; every binding mismatch and truncation refused; dynamic fields roundtrip')
