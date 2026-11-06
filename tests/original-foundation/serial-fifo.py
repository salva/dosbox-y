#!/usr/bin/env python3
"""Actual FIFO codec and ring operations: wrapped order, empty head, bad input."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
header=(root/'include/serialport.h').read_text()
source=(root/'src/hardware/serialport/serialport.cpp').read_text()
body=header[header.index('class MyFifo {'):header.index('\nenum SerialTypesE')]
codec=source[source.index('void MyFifo::saveState'):source.index('void CSerial::saveState')]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'serial-fifo'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p=Path(d)
 (p/'probe.cpp').write_text('''#include <cassert>
#include <cstdint>
#include <sstream>
using Bitu=uintptr_t;
#define INLINE inline
template<class T> void writePOD(std::ostream &s,const T &v){s.write(reinterpret_cast<const char*>(&v),sizeof(v));}
'''+body+codec+'''
int main(){
 MyFifo a(16),b(16);
 assert(a.getb()==0);
 for(unsigned n=0;n<16;++n)assert(a.addb(n));
 for(unsigned n=0;n<11;++n)assert(a.getb()==n);
 for(unsigned n=16;n<23;++n)assert(a.addb(n));
 std::stringstream s;a.saveState(s);b.loadState(s);assert(s);
 for(unsigned n=11;n<23;++n)assert(b.getb()==n);
 auto last=b.getb();assert(last==22);
 std::stringstream empty;b.saveState(empty);a.loadState(empty);
 assert(empty && a.isEmpty() && a.getb()==22);
 for(unsigned size: {0u,17u,255u}){
  std::string bad;bad+=char(size);bad+=char(0);bad+=char(1);
  std::stringstream t(bad);b.loadState(t);assert(!t);
 }
 std::stringstream bad_count(std::string("\\x01\\x02",2));b.loadState(bad_count);assert(!bad_count);
 std::stringstream short_payload(std::string("\\x10\\x02\\x55",3));b.loadState(short_payload);assert(!short_payload);
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'probe.cpp'),'-o',str(p/'probe')],check=True)
 subprocess.run([str(p/'probe')],check=True)
print('Actual FIFO wrap/empty-head/checked consumption ASan/UBSan PASS')
