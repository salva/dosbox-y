#!/usr/bin/env python3
"""Exercise actual shared component comparator, not another install engine."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'component-order'
work.mkdir(exist_ok=True)
text=(root/'src/misc/savestates.cpp').read_text()
body=text[text.index('bool SaveState::ComponentOrder::operator()'):text.index('void SaveState::registerComponent')]
source=r'''
#include <string>
#include <map>
#include <vector>
#include <cassert>
#include <algorithm>
class SaveState {public:struct ComponentOrder {bool operator()(const std::string&,const std::string&) const;};};
'''+body+r'''
int main(){
 const std::vector<std::string> expected={"BIOS layout","Memory","Shell bindings","Dos","XMS","EMS","CPU","FPU","CMOS","DMA","IO handler","Int10","IntTimer10","Keyboard","Joystick","Mouse","Vga","Mixer","Midi","Pic","Dosbox-x","Render","Extension A","Extension Z"};
 auto reversed=expected;std::reverse(reversed.begin(),reversed.end());
 std::map<std::string,int,SaveState::ComponentOrder> owners;
 for(const auto &name:reversed)owners.emplace(name,0);
 std::vector<std::string> actual;for(const auto &owner:owners)actual.push_back(owner.first);
 assert(actual==expected);
 SaveState::ComponentOrder less;
 for(const auto &a:expected){assert(!less(a,a));for(const auto &b:expected)
 for(const auto &c:expected)if(less(a,b)&&less(b,c))assert(less(a,c));}
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Shared component storage: dependency order and strict comparator passed')
