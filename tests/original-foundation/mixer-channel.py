#!/usr/bin/env python3
"""Actual mixer channel load with output-state doubles, not audio qualification."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/hardware/mixer.cpp').read_text()
a = s.index('void MixerChannel::LoadState(')
b = s.index('\nextern void POD_Save_Adlib', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mixer-channel'
w.mkdir(exist_ok=True)
code = r'''
#include <sstream>
#include <cstring>
#include <cassert>
#include <cstdint>
struct { unsigned pos=17, done=29; } mixer;
struct MixerChannel {
 float volmain[2]={1,2}, scale[2]={3,4};
 int32_t volmul[2]={5,6}; bool enabled=true;
 void LoadState(std::istream&);
};
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
''' + s[a:b] + r'''
int main() {
 float v[2]={7,8}, scale[2]={9,10};int32_t mul[2]={11,12};
 std::string bytes(reinterpret_cast<char*>(v),sizeof(v));
 bytes.append(reinterpret_cast<char*>(scale),sizeof(scale));
 bytes.append(reinterpret_cast<char*>(mul),sizeof(mul));bytes.push_back(0);
 for(size_t n=0;n<bytes.size();++n) {
  MixerChannel c;std::istringstream in(bytes.substr(0,n));c.LoadState(in);
  assert(in.fail());assert(c.volmain[0]==1 && c.volmain[1]==2);
  assert(c.scale[0]==3 && c.scale[1]==4 && c.volmul[0]==5 && c.volmul[1]==6 && c.enabled);
  assert(mixer.pos==17 && mixer.done==29);
 }
 for(unsigned n=2;n<256;++n) {
  auto bad=bytes;bad.back()=char(n);MixerChannel c;std::istringstream in(bad);c.LoadState(in);
  assert(in.fail() && c.volmain[0]==1 && c.enabled && mixer.pos==17 && mixer.done==29);
 }
 for(unsigned n=0;n<2;++n) {
  bytes.back()=char(n);MixerChannel c;std::istringstream in(bytes);c.LoadState(in);
  assert(in && c.volmain[0]==7 && c.volmain[1]==8 && c.scale[0]==9 && c.scale[1]==10);
  assert(c.volmul[0]==11 && c.volmul[1]==12 && c.enabled==bool(n));
  assert(mixer.pos==0 && mixer.done==0);
 }
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual mixer channel load every truncation/invalid boolean and output preservation ASan/UBSan PASS; output doubles, not callback qualification')
