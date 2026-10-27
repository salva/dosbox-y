#!/usr/bin/env python3
"""Original component checkpoint adapter; no emulator admission qualification."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'checkpoint-stream'
w.mkdir(exist_ok=True)
source=(root/'src/misc/savestates.cpp').read_text()
body=source[source.index('bool SaveState::checkpointSave('):source.index('void SaveState::registerComponent(')]
(w/'test.cpp').write_text(r'''
#include <map>
#include <string>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <cstdio>
#include "src/misc/state_checkpoint_stream.h"
#define C_DEBUG 0
static bool frontier_ready=true;
static bool save_state_frontier() {return frontier_ready;}
struct Fatal {};
[[noreturn]] static void E_Exit(const char *,...) {throw Fatal();}
class SaveState {
public:
 using Error=std::string;
 using Checkpoint=std::map<std::string,std::string>;
 struct Component {virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 struct Entry {Component &comp;};
 std::map<std::string,Entry> components;
 bool checkpointSave(Checkpoint &,size_t);
 bool checkpointLoad(const Checkpoint &);
};
'''+body+r'''
struct Counter:SaveState::Component {
 int value=42;bool fail=false,clear_failure=false,fatal_capture=false;unsigned installs=0;
 void getBytes(std::ostream &s) override {if(fatal_capture)throw 1;s.write((char*)&value,sizeof(value));if(clear_failure)s.clear();if(fail)throw std::runtime_error("capture");}
 void setBytes(std::istream &s) override {++installs;s.read((char*)&value,sizeof(value));}
};
int main() {
 StateCheckpointStream buffer(2);std::ostream out(&buffer);
 out<<'a'<<'b'<<'c';assert(!out);assert(buffer.failed());out.clear();out<<'x';assert(!out);assert(buffer.take()=="ab");
 SaveState state;Counter a,b;
 state.components.emplace("a",SaveState::Entry{a});state.components.emplace("b",SaveState::Entry{b});
 SaveState::Checkpoint parts={{"prior","unchanged"}};
 frontier_ready=false;assert(!state.checkpointSave(parts,8));assert(parts.at("prior")=="unchanged");
 SaveState::Checkpoint valid={{"a",std::string(sizeof(int),'x')},{"b",std::string(sizeof(int),'x')}};
 assert(!state.checkpointLoad(valid));assert(!a.installs && !b.installs);frontier_ready=true;
 assert(!state.checkpointSave(parts,7));assert(parts.at("prior")=="unchanged");
 b.clear_failure=true;assert(!state.checkpointSave(parts,7));assert(parts.at("prior")=="unchanged");b.clear_failure=false;
 b.fatal_capture=true;bool escaped=false;try {state.checkpointSave(parts,8);}catch(int){escaped=true;}
 assert(escaped && parts.at("prior")=="unchanged");b.fatal_capture=false;
 b.fail=true;assert(!state.checkpointSave(parts,8));assert(parts.at("prior")=="unchanged");
 b.fail=false;assert(state.checkpointSave(parts,8));assert(parts.size()==2);
 a.value=1;b.value=2;assert(state.checkpointLoad(parts));assert(a.value==42 && b.value==42);
 auto missing=parts;missing.erase("b");auto count=a.installs;
 assert(!state.checkpointLoad(missing));assert(a.installs==count);
 parts["b"]="x";
 bool fatal=false;try {state.checkpointLoad(parts);}catch(Fatal&){fatal=true;}assert(fatal);
 parts["b"]=std::string(sizeof(int)+1,'x');fatal=false;
 try {state.checkpointLoad(parts);}catch(Fatal&){fatal=true;}assert(fatal);
 puts("original checkpoint adapter PASS (native admission not exercised)");
}
''')
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root),str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
