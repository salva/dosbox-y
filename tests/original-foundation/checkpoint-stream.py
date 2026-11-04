#!/usr/bin/env python3
"""Bounded common capture and unconditional used-target checkpoint refusal."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'checkpoint-stream'
w.mkdir(exist_ok=True)
source=(root/'src/misc/savestates.cpp').read_text()
body=source[source.index('// Both transports use'):source.index('bool SaveState::install(')]
body+=source[source.index('struct SaveState::MemoryOutput'):source.index('bool SaveState::ComponentOrder::operator()')]
body+=source[source.index('bool SaveState::loadFile('):source.index('void SaveState::restoreFreshStartup(')]
body+=source[source.index('bool SaveState::loadFrom('):source.index('bool SaveState::isEmpty(')]
header=(root/'include/dosbox.h').read_text()
adapters=header[header.index('    struct Output {'):header.index('    bool saveTo(size_t slot,')]
(w/'test.cpp').write_text(r'''
#include <memory>
#include <map>
#include <string>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <cstdio>
#include "src/misc/state_checkpoint_stream.h"
#define C_DEBUG 0
static void LOG_MSG(const char *,...) {}
struct Section_line {std::string data;};
struct Control {Section_line devices;Section_line *GetSection(const char *) {return &devices;}} control_value;
static Control *control=&control_value;
static std::map<std::string,std::string> state_identity() {
 return {{"Archive_Revision","test-revision"},{"Build_Identity","test-build"},{"Config_Identity","test-config"}};
}
static bool frontier_ready=true;
static bool save_state_frontier() {return frontier_ready;}
class SaveState {
public:
 using Error=std::string;
 using Checkpoint=std::map<std::string,std::string>;
 struct Component {virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 struct Entry {Component &comp;};
 std::map<std::string,Entry> components;
 bool checkpointSave(Checkpoint &,size_t);
 bool checkpointLoad(const Checkpoint &);
 bool loadFile(const std::string &) const;
 bool loadFrom(size_t,const std::string *) const;
'''+adapters+r'''
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
 const size_t capacity=8+std::string("test-revisiontest-buildtest-config").size();
 SaveState state;Counter a,b;
 state.components.emplace("a",SaveState::Entry{a});state.components.emplace("b",SaveState::Entry{b});
 SaveState::Checkpoint parts={{"prior","unchanged"}};
 frontier_ready=false;assert(!state.checkpointSave(parts,capacity));assert(parts.at("prior")=="unchanged");
 SaveState::Checkpoint valid={{"a",std::string(sizeof(int),'x')},{"b",std::string(sizeof(int),'x')}};
 assert(!state.checkpointLoad(valid));assert(!a.installs && !b.installs);frontier_ready=true;
 assert(!state.checkpointSave(parts,capacity-1));assert(parts.at("prior")=="unchanged");
 b.clear_failure=true;assert(!state.checkpointSave(parts,capacity-1));assert(parts.at("prior")=="unchanged");b.clear_failure=false;
 b.fatal_capture=true;bool escaped=false;try {state.checkpointSave(parts,capacity);}catch(int){escaped=true;}
 assert(escaped && parts.at("prior")=="unchanged");b.fatal_capture=false;
 b.fail=true;assert(!state.checkpointSave(parts,capacity));assert(parts.at("prior")=="unchanged");
 b.fail=false;assert(state.checkpointSave(parts,capacity));assert(parts.size()==5);
 auto count_before=a.installs;
 for (const auto &identity:state_identity()) {
  auto wrong=parts;wrong[identity.first]="mismatch";
  assert(!state.checkpointLoad(wrong));assert(a.installs==count_before);
 }
 a.value=1;b.value=2;
 assert(!state.checkpointLoad(parts));assert(a.value==1 && b.value==2);
 auto missing=parts;missing.erase("b");auto count=a.installs;
 assert(!state.checkpointLoad(missing));assert(a.installs==count);
 parts["b"]="x";assert(!state.checkpointLoad(parts));
 parts["b"]=std::string(sizeof(int)+1,'x');assert(!state.checkpointLoad(parts));
 assert(!state.loadFile("nonexistent.sav"));
 assert(!state.loadFile(""));
 assert(!state.loadFrom(0,nullptr));
 assert(!state.loadFrom(size_t(-1),nullptr));
 assert(!a.installs && !b.installs);
 puts("bounded capture and all direct used-target imports refuse without owner effects PASS");
}
''')
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root),str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
