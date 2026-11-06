#!/usr/bin/env python3
"""Actual Disney original traversal, active FIFO and local owner identities."""
import pathlib,os,subprocess
r=pathlib.Path(__file__).resolve().parents[2];s=(r/'src/hardware/disney.cpp').read_text()
layout=s[s.index('#define DISNEY_SIZE'):s.index('#define DS_FINISH')]
codec=s[s.index('void POD_Save_Disney'):]
code='''#include <sstream>
#include <cstring>
#include <cassert>
#include <cstdint>
using Bitu=uintptr_t;
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
struct MixerObject{};struct MixerChannel{void SaveState(std::ostream &s){s.put('x');}void LoadState(std::istream &s){if(s.get()!='x')s.setstate(std::ios::failbit);}};
bool test=true;
'''+layout+'\n#define DS_ANALYZING 3\n'+codec+'''
int main(){MixerObject mo1,mo2;MixerChannel ch1,ch2;
 disney.mo=&mo1;disney.chan=&ch1;disney.da[1].used=3;
 disney.da[1].buffer[2]=77;disney.da[1].speedcheck_last=1.25;
 disney.da[1].last_dac=51;disney.leader=&disney.da[1];disney.state=3;
 std::ostringstream out;POD_Save_Disney(out);assert(out);auto bytes=out.str();
 disney={};disney.mo=&mo2;disney.chan=&ch2;
 std::istringstream in(bytes);POD_Load_Disney(in);assert(in);
 assert(disney.mo==&mo2&&disney.chan==&ch2&&disney.leader==&disney.da[1]);
 assert(disney.da[1].buffer[2]==77&&disney.da[1].used==3&&disney.da[1].speedcheck_last==1.25);
 std::ostringstream again;POD_Save_Disney(again);assert(again.str()==bytes);
 for(size_t n=0;n<bytes.size();++n){std::istringstream shortin(bytes.substr(0,n));POD_Load_Disney(shortin);assert(!shortin);}
 auto bad=bytes;bad[bad.size()-2]=2;std::istringstream invalid(bad);POD_Load_Disney(invalid);assert(!invalid);
 disney.leader=reinterpret_cast<dac_channel*>(&mo1);std::ostringstream unknown;POD_Save_Disney(unknown);assert(!unknown);
}
'''
w=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'disney-owner-test';w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-g',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual Disney codec active FIFO/local identities/every truncation/leader rejection passed')
