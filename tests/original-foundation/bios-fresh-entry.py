#!/usr/bin/env python3
"""Exercise actual fresh BIOS entry admission/order with native helper stubs."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'bios-fresh-entry'
work.mkdir(exist_ok=True)
s=(root/'src/ints/bios.cpp').read_text()
body=s[s.index('    static void ConstructFreshNative() {'):s.index('    void write_FFFF_signature()')]
source=r'''
#include <cassert>
#include <stdexcept>
#include <vector>
bool bios_first_init=true;int bios_post_counter=0;void *first_shell=nullptr;unsigned bios_user_reset_vector_blob=0;
void E_Exit(const char *s){throw std::runtime_error(s);}
std::vector<int> calls;
struct BIOS {
 static void ConstructPOSTPrefixNative(){calls.push_back(1);++bios_post_counter;}
 static void ConstructPOSTNative(){calls.push_back(2);bios_first_init=false;}
'''+body+r'''};
bool rejected(){try{BIOS::ConstructFreshNative();return false;}catch(const std::runtime_error&){return true;}}
int main(){
 bios_user_reset_vector_blob=16;assert(rejected() && calls.empty());bios_user_reset_vector_blob=0;
 first_shell=&calls;assert(rejected() && calls.empty());first_shell=nullptr;
 bios_post_counter=1;assert(rejected() && calls.empty());bios_post_counter=0;
 bios_first_init=false;assert(rejected() && calls.empty());bios_first_init=true;
 BIOS::ConstructFreshNative();assert((calls==std::vector<int>{1,2}));assert(rejected() && calls.size()==2);
}
'''
p=work/'entry.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(work/'entry')],check=True)
subprocess.run([str(work/'entry')],check=True)
print('Fresh BIOS entry: used/reset-blob/shell refusal before construction; prefix then native body once (stubbed helper effects)')
