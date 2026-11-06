#!/usr/bin/env python3
"""Compile the actual MixerChannel owner codec against a small native ABI double."""
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[2]
src=(root/'src/hardware/mixer.cpp').read_text()
codec=src[src.index('#define MIXER_CONTINUATION_FIELDS'):src.index('extern void POD_Save_Adlib')]
h=(root/'include/mixer.h').read_text()
fields=h[h.index('\tMIXER_Handler handler;'):h.index('\n};',h.index('\tMIXER_Handler handler;'))]
code='''#include <sstream>
#include <cassert>
#include <cstring>
#include <cstdint>
using Bitu=uintptr_t;using Bits=intptr_t;using MIXER_Handler=void(*)(Bitu);
#define LOWPASS_ORDER 8
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
class MixerChannel {public:void SaveState(std::ostream&);void LoadState(std::istream&);
'''+fields+'\n};\n'+codec+'''
void a(Bitu){} void b(Bitu){}
int main(){
 MixerChannel x={},y={};x.freq_n=44100;x.freq_d=x.freq_d_orig=1000;
 x.freq_nslew=900;x.lowpass_order=2;x.lowpass_alpha=317;x.lowpass[0][0]=4711;
 x.current_loaded=true;x.current[1]=817;x.last[0]=-55;x.delta[0]=172;
 x.msbuffer_o=7;x.msbuffer_i=3;x.msbuffer[5][1]=-913;
 x.freq_fslew=29;x.rend_n=3;x.rend_d=61;x.enabled=true;
 x.handler=a;x.name="source";y.handler=b;y.name="destination";y.next=&x;
 std::ostringstream out;x.SaveState(out);assert(out);
 auto bytes=out.str();std::istringstream in(bytes);y.LoadState(in);assert(in);
 assert(y.handler==b&&y.next==&x&&!strcmp(y.name,"destination"));
 std::ostringstream again;y.SaveState(again);assert(again.str()==bytes);
 assert(y.msbuffer[5][1]==-913&&y.lowpass[0][0]==4711&&y.rend_d==61);
 for(size_t n=0;n<bytes.size();++n){auto before=y.current[1];
  std::istringstream shortin(bytes.substr(0,n));y.LoadState(shortin);
  assert(!shortin&&y.current[1]==before&&y.handler==b);}
 bytes.back()=2;std::istringstream bad(bytes);y.LoadState(bad);assert(!bad);
 x.msbuffer_o=2049;std::ostringstream invalid;x.SaveState(invalid);assert(!invalid);
}
'''
work=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mixer-channel-test';work.mkdir(exist_ok=True)
(work/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('actual MixerChannel codec: nonzero history, destination identity, every truncation, bool and bounds passed')
