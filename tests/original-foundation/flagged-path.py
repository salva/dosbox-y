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
fixture = '#include <string>\n#include <cassert>\n' + body + r'''
int main() {
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
subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', str(work/'probe.cpp'), '-o', str(work/'probe')], check=True)
subprocess.run([str(work/'probe')], check=True)
print('flagged sidecar host-path separation PASS')
