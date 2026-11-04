#!/usr/bin/env python3
"""Exercise the actual registered POD reader, including malformed bool bytes.

Reduced harness, not emulator restore or ABI compatibility qualification.
"""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'pod-consumption'
work.mkdir(exist_ok=True)
text = (root/'include/dosbox.h').read_text()
start = text.index('class SerializeGlobalPOD :')
end = text.index('//---------------- inline implementation', start)
source = r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
class SaveState {
public:
 struct Component { virtual ~Component()=default;
 virtual void getBytes(std::ostream&)=0; virtual void setBytes(std::istream&)=0; };
 static SaveState& instance() {static SaveState s;return s;}
 void registerComponent(const std::string&,Component&) {}
};
'''+text[start:end]+r'''
struct Owner:SerializeGlobalPOD {
 bool flag=false; unsigned value=7;
 Owner():SerializeGlobalPOD("fixture") {registerPOD(flag);registerPOD(value);}
 using SerializeGlobalPOD::getBytes; using SerializeGlobalPOD::setBytes;
};
int main() {
 Owner o;
 for (bool b : {false,true}) {
  o.flag=b;std::ostringstream out;o.getBytes(out);
  o.flag=!b;o.value=9;std::istringstream in(out.str());o.setBytes(in);
  assert(in && o.flag==b && o.value==7);
 }
 for(unsigned byte=2;byte<256;++byte) {
  bool no=false,yes=true; char invalid[sizeof(bool)]={};invalid[0]=byte;
  if(!memcmp(invalid,&no,sizeof(bool)) || !memcmp(invalid,&yes,sizeof(bool))) continue;
  o.flag=true;o.value=7;std::istringstream in(std::string(invalid,sizeof(invalid)));
  o.setBytes(in);assert(in.fail() && o.flag && o.value==7);
 }
 std::istringstream empty("");o.setBytes(empty);assert(empty.fail());
}
'''
path=work/'consumer.cpp'
path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('POD consumer: valid bool roundtrips, invalid bool rejection and EOF passed')
