#!/usr/bin/env python3
"""Actual Tandy traversal, DMA and destination-local native identities."""
import pathlib,os,subprocess
r=pathlib.Path(__file__).resolve().parents[2];s=(r/'src/hardware/tandy_sound.cpp').read_text()
layout=s[s.index('static struct {'):s.index('} tandy;')+len('} tandy;')]
codec=s[s.index('void POD_Save_Tandy_Sound'):]
code='''#include <sstream>
#include <cstring>
#include <cassert>
#include <cstdint>
using Bitu=uintptr_t;
#define TDAC_DMA_BUFSIZE 1024
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
struct MixerChannel{void SaveState(std::ostream &s){s.put('x');}void LoadState(std::istream &s){if(s.get()!='x')s.setstate(std::ios::failbit);}};
struct DmaChannel{bool DMA16=false;} channels[8];
DmaChannel *GetDMAChannel(unsigned n){return n<8?&channels[n]:nullptr;}
MixerChannel chip;MixerChannel *activeDevice=&chip;
bool test=true;
'''+layout+codec+'''
int main(){MixerChannel ch1,ch2,ch3,ch4;
 tandy.chan=&ch1;tandy.dac.chan=&ch2;tandy.dac.dma.chan=&channels[1];
 tandy.dac.enabled=true;tandy.dac.dma.buf[13]=91;tandy.dac.dma.last_sample=45;
 tandy.dac.dma.rate=11025;tandy.dac.frequency=50;tandy.dac.mode=3;
 std::ostringstream out;POD_Save_Tandy_Sound(out);assert(out);auto bytes=out.str();
 tandy={};tandy.chan=&ch3;tandy.dac.chan=&ch4;
 std::istringstream in(bytes);POD_Load_Tandy_Sound(in);assert(in);
 assert(tandy.chan==&ch3&&tandy.dac.chan==&ch4&&tandy.dac.dma.chan==&channels[1]);
 assert(tandy.dac.dma.buf[13]==91&&tandy.dac.dma.last_sample==45&&tandy.dac.dma.rate==11025);
 std::ostringstream again;POD_Save_Tandy_Sound(again);assert(again.str()==bytes);
 for(size_t n=0;n<bytes.size();++n){std::istringstream shortin(bytes.substr(0,n));POD_Load_Tandy_Sound(shortin);assert(!shortin);}
 auto bad=bytes;bad[bad.size()-4]=7;std::istringstream invalid(bad);POD_Load_Tandy_Sound(invalid);assert(!invalid);
 tandy.dac.dma.chan=nullptr;std::ostringstream no_dma;POD_Save_Tandy_Sound(no_dma);assert(!no_dma);
}
'''
w=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'tandy-owner-test';w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-g',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual Tandy codec active backing/local identities/every truncation/DMA rejection passed')
