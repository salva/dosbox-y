#!/usr/bin/env python3
"""Actual native startup wrapper admission/order; helpers stubbed, not runtime proof."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'fresh-native-entry'
work.mkdir(exist_ok=True)
s=(root/'src/gui/sdlmain.cpp').read_text()
body=s[s.index('void VM_ConstructFreshNative() {'):s.index('#if defined(LINUX) && C_X11',s.index('void VM_ConstructFreshNative() {'))]
source=r'''
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
#include "native_frontier.h"
struct Section {virtual ~Section()=default;};
struct Section_line:Section {std::string data;};
struct Control {Section_line devices;Section *GetSection(const char *){return &devices;}} configuration;
Control *control=&configuration;
bool guest_machine_power_on=true,dos_kernel_disabled=true,boot_shell_pending=false;
void *first_shell=nullptr;
std::vector<int> calls;int fail_stage=0;
void E_Exit(const char *s){throw std::runtime_error(s);}
void stage(int n){calls.push_back(n);if(n==fail_stage)throw std::runtime_error("native construction failed");}
void BIOS_ConstructFreshNative(){stage(1);}
void VM_ConstructDOSKernelNative(bool initialize_guest_clock){assert(!initialize_guest_clock);stage(2);}
void SHELL_MessagesInit(){stage(3);}
void VM_ConstructDOSShellNative(){stage(4);}
'''+body+r'''
bool rejected(){try{VM_ConstructFreshNative();return false;}catch(const std::runtime_error&){return true;}}
int main(int argc,char **argv){
 if(argc>1 && std::string(argv[1]).find("fail")==0){
  fail_stage=std::stoi(std::string(argv[1]).substr(4));
  assert(rejected() && calls.size()==static_cast<unsigned>(fail_stage));
  const auto partial=calls;fail_stage=0;
  assert(rejected() && calls==partial);return 0;
 }
 if(argc>1){
  configuration.devices.data=argc>1 && std::string(argv[1])=="newline"?"\n":"device=driver.sys";assert(rejected() && calls.empty());
  configuration.devices.data="";assert(rejected() && calls.empty());return 0;
 }
 guest_machine_power_on=false;assert(rejected() && calls.empty());guest_machine_power_on=true;
 {NativeFrontier::Frontiers::Scope decoder(NativeFrontier::frontiers(),true);
  assert(rejected() && calls.empty());}
 {NativeFrontier::Frontiers::Scope callback(NativeFrontier::frontiers(),false);
  assert(rejected() && calls.empty());}
 dos_kernel_disabled=false;assert(rejected() && calls.empty());dos_kernel_disabled=true;
 first_shell=&calls;assert(rejected() && calls.empty());first_shell=nullptr;
 boot_shell_pending=true;assert(rejected() && calls.empty());boot_shell_pending=false;
 configuration.devices.data=" \t";
 VM_ConstructFreshNative();assert((calls==std::vector<int>{1,2,3,4}));
 assert(rejected() && calls.size()==4);
}
'''
p=work/'consumer.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'include'),str(p),'-o',str(work/'consumer')],check=True)
for args in [[],['devices'],['newline'],*([f'fail{n}'] for n in range(1,5))]:
 subprocess.run([str(work/'consumer'),*args],check=True)
print('Fresh native wrapper: admission, native order, unsupported devices and one-shot failure passed (stub helpers)')
