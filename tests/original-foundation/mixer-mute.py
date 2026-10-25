#!/usr/bin/env python3
"""Compile actual mixer startup reads; no host playback qualification."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
source = (root/'src/hardware/mixer.cpp').read_text()
a = source.index('    /* Read out config section */')
b = source.index('    /* Initialize the internal stuff */', a)
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mixer-mute'
work.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <cstring>
struct Section {
 bool mute=false, nosound=false;
 int Get_int(const char*) {return 48000;}
 bool Get_bool(const char *name) {
  if(!strcmp(name,"mute"))return mute;
  if(!strcmp(name,"nosound"))return nosound;
  return false;
 }
};
struct Control {bool opt_silent=false;} state, *control=&state;
struct {
 unsigned freq,blocksize;bool nosound,swapstereo,sampleaccurate,dc_bias_adj,mute;
} mixer;
void initialize(Section *section) {
''' + source[a:b] + r'''
}
int main() {
 for(unsigned mute=0;mute<2;++mute)
 for(unsigned nosound=0;nosound<2;++nosound)
 for(unsigned silent=0;silent<2;++silent) {
  Section section;section.mute=mute;section.nosound=nosound;
  state.opt_silent=silent;
  mixer.mute=!mute;mixer.nosound=!nosound;
  initialize(&section);
  assert(mixer.mute==bool(mute));
  assert(mixer.nosound==bool(nosound||silent));
 }
}
'''
(work/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual mixer startup mute/nosound/silent independence: eight combinations ASan/UBSan PASS; not playback qualification')
