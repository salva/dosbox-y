#!/usr/bin/env python3
"""Original POD owner short-read publication, not archive validation."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'pod-owner-regression'
work.mkdir(exist_ok=True)
t=(root/'include/dosbox.h').read_text()
a=t.index('class SerializeGlobalPOD :')
b=t.index('//---------------- inline implementation',a)
s=r'''
#include <algorithm>
#include <cassert>
#include <sstream>
#include <string>
#include <vector>
struct SaveState {
 struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component&){}
};
'''+t[a:b]+r'''
struct Owner : SerializeGlobalPOD {
 unsigned a=0x12345678,b=0xabcdef01;
 Owner():SerializeGlobalPOD("test"){registerPOD(a);registerPOD(b);}
 using SerializeGlobalPOD::getBytes;using SerializeGlobalPOD::setBytes;
};
int main(){
 Owner source;std::ostringstream out;source.getBytes(out);const auto bytes=out.str();
 for(size_t n=0;n<bytes.size();++n){
  Owner dest;dest.a=17;dest.b=23;std::istringstream in(bytes.substr(0,n));
  dest.setBytes(in);assert(in.fail());assert(dest.a==17 && dest.b==23);
 }
 Owner dest;dest.a=17;dest.b=23;std::istringstream in(bytes);
 dest.setBytes(in);assert(!in.fail());assert(dest.a==source.a && dest.b==source.b);
 std::istringstream failed(bytes);failed.setstate(std::ios::failbit);
 dest.a=99;dest.setBytes(failed);assert(dest.a==99);
 Owner throwing;throwing.a=17;throwing.b=23;
 std::istringstream short_in(bytes.substr(0,bytes.size()-1));
 short_in.exceptions(std::ios::failbit|std::ios::badbit);
 bool caught=false;try{throwing.setBytes(short_in);}catch(const std::ios_base::failure&){caught=true;}
 assert(caught && throwing.a==17 && throwing.b==23);
}
'''
loader=(root/'src/misc/savestates.cpp').read_text()
x=loader.index('\t\ti->second.comp.setBytes(ss);')
y=loader.index('if ((err=zis.close())',x)
install=loader[x:y]
s += '\n' # loader fatal seam is checked separately with a throwing fatal stand-in
install=install.replace('i->second.comp.setBytes(ss);','owner.setBytes(ss);').replace('i->first.c_str()','"test"')
s=s.replace('int main(){',r'''
[[noreturn]] void E_Exit(const char*,const char*){throw 7;}
void install_owner(Owner &owner,std::istream &ss){
'''+install+r'''
}
int main(){
 {Owner owner;std::istringstream ss("x");bool fatal=false;
  try{install_owner(owner,ss);}catch(int n){fatal=n==7;}assert(fatal);}
''')
tail_start=loader.index('\t// CRC/close failures or missing later components')
tail=loader[tail_start:loader.index('\n}',tail_start)]
s += '\nunsigned completions=0;\nvoid flagged_restore(char*){++completions;}\n'
s += '[[noreturn]] void E_Exit(const char*){throw 8;}\n'
s += 'void finish(bool load_err,bool installation_started){bool dos_kernel_disabled=false;std::string save="test";\n'
# The log is presentation only; retain the actual completion/fatal decisions.
s += tail[:tail.index('\tif (!load_err) LOG_MSG')]+'\n}\n'
s += '''
struct CheckFinish {CheckFinish(){
 finish(true,false);assert(completions==0);
 bool fatal=false;try{finish(true,true);}catch(int n){fatal=n==8;}
 assert(fatal && completions==0);
 finish(false,true);assert(completions==1);
}} check_finish;
'''
(work/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('POD owner short-read ASan/UBSan PASS (not whole-archive validation)')
