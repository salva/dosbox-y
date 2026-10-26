#!/usr/bin/env python3
"""Compile the production sidecar path primitive; no emulator doubles."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'flagged-path-regression'
work.mkdir(exist_ok=True)
source = (root / 'src/dos/dos_programs.cpp').read_text()
body = source[source.index('static std::string flagged_sidecar_path('):source.index('int flagged_backup(')]
fixture = '#include <string>\n#include <cstdint>\n#include <cassert>\n#include "state_file_io.h"\n' + body + r'''
int main() {
 const auto stamp = [](unsigned year, unsigned month, unsigned day,
                       unsigned hour, unsigned minute, unsigned second) -> uint32_t {
  return ((year-1980)<<25) | (month<<21) | (day<<16) |
         (hour<<11) | (minute<<5) | (second/2);
 };
 assert(flagged_timestamp_valid(stamp(1980,1,1,0,0,0)));
 assert(flagged_timestamp_valid(stamp(2107,12,31,23,59,58)));
 assert(flagged_timestamp_valid(stamp(2000,2,29,0,0,0)));
 assert(!flagged_timestamp_valid(stamp(2100,2,29,0,0,0)));
 assert(!flagged_timestamp_valid(stamp(2026,2,29,0,0,0)));
 assert(!flagged_timestamp_valid(stamp(2026,4,31,0,0,0)));
 assert(!flagged_timestamp_valid(stamp(2026,0,1,0,0,0)));
 assert(!flagged_timestamp_valid(stamp(2026,13,1,0,0,0)));
 assert(!flagged_timestamp_valid(stamp(2026,1,0,0,0,0)));
 assert(!flagged_timestamp_valid(stamp(2026,1,1,24,0,0)));
 assert(!flagged_timestamp_valid(stamp(2026,1,1,0,60,0)));
 assert(!flagged_timestamp_valid(stamp(2026,1,1,0,0,60)));
 assert(!StateFileIO::active());
 {
  StateFileIO::Scope outer;
  assert(StateFileIO::active());
  try { StateFileIO::Scope inner; throw 1; } catch (int) {}
  assert(StateFileIO::active());
 }
 assert(!StateFileIO::active());
 assert(flagged_sidecar_path("/tmp/first.sav") == "/tmp/first.dat");
 assert(flagged_sidecar_path("/tmp/first") == "/tmp/first.dat");
 assert(flagged_sidecar_path("/tmp/first.SAV") == "/tmp/first.SAV.dat");
 assert(flagged_sidecar_path("/tmp/a.sav.dir/first") == "/tmp/a.sav.dir/first.dat");
 assert(flagged_sidecar_path("C:\\a.sav.dir\\first") == "C:\\a.sav.dir\\first.dat");
 const auto long_path = std::string(8192, 'x');
 assert(flagged_sidecar_path(long_path.c_str()) == long_path + ".dat");
}
'''
(work / 'probe.cpp').write_text(fixture)
subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', str(root/'include'), str(work/'probe.cpp'), '-o', str(work/'probe')], check=True)
subprocess.run([str(work/'probe')], check=True)
print('flagged sidecar host-path separation PASS')
