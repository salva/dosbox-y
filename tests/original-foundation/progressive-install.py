#!/usr/bin/env python3
"""Actual common install traversal with reduced transport/owners, not startup proof."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'progressive-install';w.mkdir(exist_ok=True)
text=(root/'src/misc/savestates.cpp').read_text()
body=text[text.index('bool SaveState::install(Input &input) const {'):text.index('struct SaveState::MemoryOutput')]
source=r'''
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cassert>
#include <vector>
#define C_DEBUG 0
#define C_DEBUG_LUA 0
struct Fatal {};
void E_Exit(const char *,...){throw Fatal{};}
void LOG_MSG(const char *,...){}
std::map<std::string,std::string> state_identity(){return {{"identity","engine"}};}
class SaveState {
public:
 struct Error {};
 struct Input {virtual ~Input()=default;virtual bool verify(const std::string&)=0;virtual std::istream *open(const std::string&)=0;virtual bool close()=0;};
 struct Component {unsigned calls=0;bool fail=false,throws=false;
  void setBytes(std::istream &s){++calls;if(throws)throw std::runtime_error("owner");char c;s.get(c);if(fail)s.setstate(std::ios::failbit);}};
 struct Entry {Component &comp;};
 std::map<std::string,Entry> components;
 bool install(Input&) const;
};
'''+body+r'''
struct Transport:SaveState::Input {
 std::map<std::string,std::string> parts{{"identity","engine"},{"a","a"},{"b","b"}};
 std::istringstream stream;std::string current,bad_close,bad_open;
 std::vector<std::string> opens,verifies;
 bool verify(const std::string &n)override{verifies.push_back(n);return parts.count(n);}
 std::istream *open(const std::string &n)override{
  opens.push_back(n);current=n;if(n==bad_open || !parts.count(n))return nullptr;
  stream.clear();stream.str(parts.at(n));return &stream;
 }
 bool close()override{return current!=bad_close;}
};
int main(){
 for(unsigned kind=0;kind<14;++kind){
  SaveState::Component a,b;SaveState state;state.components.emplace("a",SaveState::Entry{a});state.components.emplace("b",SaveState::Entry{b});
  Transport t;
  if(kind==1)t.parts.erase("b");
  if(kind==2)t.parts["b"]="";
  if(kind==3)t.parts["b"]="extra";
  if(kind==4)t.bad_close="b";
  if(kind==5)b.fail=true;
  if(kind==6)b.throws=true;
  if(kind==7)t.parts["identity"]="wrong!";
  if(kind==8)t.parts.erase("identity");
  if(kind==9)t.bad_open="identity";
  if(kind==10)t.parts["identity"]="eng";
  if(kind==11)t.parts["identity"]="engine-extra";
  if(kind==12)t.bad_close="identity";
  if(kind==13)t.bad_open="b";
  bool fatal=false,ok=false;try{ok=state.install(t);}catch(const Fatal &){fatal=true;}
  assert(t.verifies==std::vector<std::string>{"identity"});
  if(kind==0){assert(ok && !fatal && a.calls==1 && b.calls==1);}
  else if(kind>=7 && kind<=12){assert(!ok && !fatal && !a.calls && !b.calls);}
  else {assert(fatal && !ok && a.calls==1);assert(b.calls==((kind==1 || kind==13)?0u:1u));}
 }
}
'''
p=w/'consumer.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(w/'consumer')],check=True)
subprocess.run([str(w/'consumer')],check=True)
print('Actual install traversal: metadata missing/open/short/trailing/close refuses before owners; progressive owner missing/open/short/trailing/close/errors fail fatally; reduced transport only')
