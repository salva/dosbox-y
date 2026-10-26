#!/usr/bin/env python3
"""Consumed El Torito reuse identity; extracted decision, native devices not mocked as proof."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text()
a = s.index('        const auto *torito =')
b = s.index('        if (reuse_image)', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'bios-reuse'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <string>
#include <strings.h>
struct imageDisk {virtual ~imageDisk()=default;std::string diskname="image";};
struct imageDiskElToritoFloppy:imageDisk {unsigned CDROM_drive=2,cdrom_sector_offset=17,floppy_type=3;};
struct Options {unsigned mounttype=1,CDROM_drive=2,cdrom_sector_offset=17,floppy_emu_type=3;}opts;
imageDisk* imageDiskList[6]={};
bool reuse(){const int d=4;const char* diskname="image";
DECISION
return reuse_image;
}
int main(){
 assert(!reuse());
 imageDisk other;imageDiskList[4]=&other;assert(!reuse());
 imageDiskElToritoFloppy torito;imageDiskList[4]=&torito;assert(reuse());
 ++opts.CDROM_drive;assert(!reuse());--opts.CDROM_drive;
 ++opts.cdrom_sector_offset;assert(!reuse());--opts.cdrom_sector_offset;
 ++opts.floppy_emu_type;assert(!reuse());--opts.floppy_emu_type;assert(reuse());
 // Non-El-Torito name-only semantics remain explicitly unqualified.
 opts.mounttype=0;imageDiskList[4]=&other;assert(reuse());
 other.diskname="different";assert(!reuse());
}
'''.replace('DECISION',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual BIOS El Torito reuse decision ASan/UBSan PASS; not media/device qualification')
