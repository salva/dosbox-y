#!/usr/bin/env python3
"""Exercise the actual LocalFile timestamp callback boundary, not a copy."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'state-file-time-regression'
work.mkdir(exist_ok=True)
source = (root/'src/dos/drive_local.cpp').read_text()
body = source[source.index('bool LocalFile::UpdateLocalDateTime(void) {'):source.index('void LocalFile::Flush(void) {')]
fixture = r'''
#include <cassert>
#include <cstdint>
#include <ctime>
#include <stdexcept>
#include "state_file_io.h"
uint16_t reg_ax=17, reg_cx=18, reg_dx=19;
uint8_t reg_ah=0, reg_dh=0, reg_dl=0, reg_ch=0, reg_cl=0;
unsigned callbacks=0;
[[noreturn]] void E_Exit(const char*) { throw std::runtime_error("fatal transfer"); }
void CALLBACK_RunRealInt(unsigned n) {
 assert(n==0x21);
 ++callbacks;
 if (reg_ah==0x2a) {reg_cx=2026;reg_dh=10;reg_dl=26;}
 else {assert(reg_ah==0x2c);reg_ch=12;reg_cl=34;reg_dh=56;}
}
struct LocalFile {
 unsigned time=0,date=0;
 bool newtime=false;
 bool UpdateLocalDateTime();
};
'''+body+r'''
int main() {
 LocalFile file;
 {
  StateFileIO::Scope transfer;
  try {file.UpdateLocalDateTime();assert(false);}
  catch(const std::runtime_error&) {}
  assert(callbacks==0 && !file.newtime && file.time==0 && file.date==0);
  assert(reg_ax==17 && reg_cx==18 && reg_dx==19);
 }
 assert(!StateFileIO::active());
 assert(file.UpdateLocalDateTime());
 assert(callbacks==2 && file.newtime);
 assert(reg_ax==17 && reg_cx==18 && reg_dx==19);
 assert(file.time==((12u<<11)|(34u<<5)|28u));
 assert(file.date==((46u<<9)|(10u<<5)|26u));
}
'''
(work/'probe.cpp').write_text(fixture)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root/'include'),str(work/'probe.cpp'),'-o',str(work/'probe')],check=True)
subprocess.run([str(work/'probe')],check=True)
print('actual LocalFile timestamp: stopped transfer fatal before callback; guest callbacks preserved PASS')
