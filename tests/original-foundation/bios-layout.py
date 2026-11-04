#!/usr/bin/env python3
"""Extract the real ROM layout consumer; no actual BIOS boot/restore claim."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'bios-layout'
work.mkdir(exist_ok=True)
h=(root/'include/dosbox.h').read_text()
pod=h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
t=(root/'src/ints/bios.cpp').read_text()
codec=t[t.index('namespace {\nclass SerializeBIOSLayout'):t.index('static bool ACPI_SCI_EN')]
source=r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+r'''
struct Block {std::string who;uint32_t start,end;bool free,fixed;};
struct {uint32_t _min=0xe0000,_max=0xfffff,_max_nonfixed=0xff000;bool topDownAlloc=true;
 std::vector<Block> alist={{"INT15",0xe0100,0xe011f,false,true},{"free",0xe0120,0xfffff,true,false}};} rombios_alloc;
uint32_t rombios_minimum_location=0xe0000,rombios_minimum_size=0x10000;
'''+codec+r'''
std::string save(){std::ostringstream s;bios_layout_codec.getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);bios_layout_codec.setBytes(s);return bool(s);}
int main(){const auto bytes=save();assert(load(bytes));
 for(size_t n=0;n<bytes.size();++n){assert(!load(bytes.substr(0,n)));
 auto bad=bytes;bad[n]^=1;assert(!load(bad));}
 auto original=rombios_alloc.alist;
 rombios_alloc.alist[0].who="INT16";assert(!load(bytes));rombios_alloc.alist=original;
 std::swap(rombios_alloc.alist[0],rombios_alloc.alist[1]);assert(!load(bytes));rombios_alloc.alist=original;
 assert(load(bytes) && save()==bytes);
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('BIOS layout: every byte mismatch/truncation, role/order/location/span refuses without replacing native layout')
