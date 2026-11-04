#!/usr/bin/env python3
"""Run actual capture admission prefix; reduced configuration, no guest startup."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'capture-recipe'
work.mkdir(exist_ok=True)
text=(root/'src/misc/savestates.cpp').read_text()
a=text.index('bool SaveState::capture(Output &output) const {')
b=text.index('    for (const auto &item : state_identity())',a)
body=text[a:b]+'    (void)output; return true;\n}\n'
source=r'''
#include <string>
#include <cassert>
struct Section {virtual ~Section()=default;};
struct Section_line:Section {std::string data;};
struct Control {Section *section=nullptr;Section *GetSection(const char*){return section;}} owner,*control=&owner;
#define LOG_MSG(...) ((void)0)
struct SaveState {struct Output {};bool capture(Output&) const;};
'''+body+r'''
int main(){SaveState state;SaveState::Output output;
 assert(state.capture(output));
 Section_line devices;owner.section=&devices;
 for(const char *value:{""," ","\t"}){devices.data=value;assert(state.capture(output));}
 for(const char *value:{"\t\r\n","device=foo.sys","\n device=foo.sys\n","rem a line"}){
 devices.data=value;assert(!state.capture(output));}
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Capture recipe: absent/empty devices admitted, all nonempty devices refused; reduced config only')
