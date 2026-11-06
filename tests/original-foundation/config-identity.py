#!/usr/bin/env python3
"""Compile actual config identity with reduced section/property doubles."""
import os, pathlib, subprocess
root=pathlib.Path(__file__).resolve().parents[2]
w=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'config-identity';w.mkdir(exist_ok=True)
source=(root/'src/misc/savestates.cpp').read_text()
body=source.split('static std::string state_config_identity() {',1)[1].split('static const char state_archive_revision',1)[0]
code=r'''
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
namespace CapturePNG {struct SHA256 {std::string s;void update(const uint8_t*p,size_t n){s.append((const char*)p,n);}void finish(char *p){ // deterministic noncryptographic double
 uint64_t h=0;for(unsigned char c:s)h=h*131+c;
 for(unsigned n=0;n<64;++n){p[n]="0123456789abcdef"[(h>>((n%16)*4))&15];}
 p[64]=0;
}};}
struct Value{std::string s;std::string ToString(){return s;}};
struct Property{std::string propname;Value value;Value GetValue(){return value;}};
struct Section{virtual ~Section(){};virtual const char*GetName()=0;};
struct Section_prop:Section {std::string name;std::vector<Property> props;const char*GetName()override{return name.c_str();}Property*Get_prop(int i){return unsigned(i)<props.size()?&props[i]:nullptr;}};
struct Control{std::vector<Section*>sections;Section*GetSection(int i){return unsigned(i)<sections.size()?sections[i]:nullptr;}} owner,*control=&owner;
unsigned resolved_pages=4096;
unsigned MEM_TotalPages(){return resolved_pages;}
unsigned MEM_TotalPagesAt4GB(){return 0;}
std::string BIOS_StateConfigIdentity(){return "initialized BIOS";}
uint32_t CPU_StateFpuIdentity(){return 0x100;}
std::string CPU_StateCpuidVendor(){return "GenuineIntel";}
const std::string &CPU_StateConfigIdentity(){static const std::string value="resolved";return value;}
uint32_t CPU_StateArchitectureIdentity(){return 0xfe;}
std::string CPU_StateSerialIdentity(){return "disabled";}
unsigned rep_identity=4;
uint32_t CPU_StateRepIdentity(){return rep_identity;}
unsigned instruction_identity=7;
uint32_t CPU_StateInstructionIdentity(){return instruction_identity;}
unsigned pse_identity=0x104;
uint32_t CPU_StatePseIdentity(){return pse_identity;}
std::string STATE_MachineInputsIdentity(){return "loaded-rom-identity";}
std::string identity(){
'''+body+r'''
int main(){
 Section_prop machine;machine.name="dosbox";
 machine.props={{"memsize",{"16"}},{"saveslot",{"1"}},{"savefile",{"first.sav"}},{"saveremark",{"true"}},{"autosave",{"10 1-3"}},{"forceloadstate",{"false"}}};
 owner.sections={&machine};auto baseline=identity();
 for(unsigned i=1;i<machine.props.size();++i){auto old=machine.props[i].value.s;machine.props[i].value.s="different-policy";assert(identity()==baseline);machine.props[i].value.s=old;}
 machine.props[0].value.s="32";assert(identity()==baseline);
 resolved_pages=8192;assert(identity()!=baseline);
 resolved_pages=4096;
 Section_prop cpu;cpu.name="cpu";cpu.props={{"enable pse",{"auto"}}};
 owner.sections.push_back(&cpu);baseline=identity();
 cpu.props[0].value.s="pse36";assert(identity()==baseline);
 pse_identity=0x100;assert(identity()!=baseline);
 pse_identity=0;assert(identity()!=baseline);
 pse_identity=0x104;
 cpu.props={{"enable msr",{"false"}},{"enable syscall",{"true"}},
 {"enable cmpxchg8b",{"true"}},
 {"mask stack pointer for enter leave instructions",{"auto"}},
 {"allow lmsw to exit protected mode",{"auto"}},
 {"interruptible rep string op",{"-1"}}};
 instruction_identity=4;baseline=identity();
 cpu.props[1].value.s="false";assert(identity()==baseline);
 cpu.props[3].value.s="false";cpu.props[4].value.s="false";
 assert(identity()==baseline);
 cpu.props[5].value.s="4";assert(identity()==baseline);
 rep_identity=8;assert(identity()!=baseline);rep_identity=4;
 for(unsigned changed:{0u,5u,7u,12u,20u}){
   instruction_identity=changed;assert(identity()!=baseline);
 }
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-fsanitize=undefined',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual identity traversal ignores capture/live-load policy and requested RAM spelling; retains resolved memory distinction (reduced hash/section doubles, not effective-hardware completeness)')
