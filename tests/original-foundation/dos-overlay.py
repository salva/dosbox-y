#!/usr/bin/env python3
"""Exercise the actual consumed overlay replacement span with native-owner doubles."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text()
a = s.index('                            // Retain the base until its native replacement succeeds.')
b = s.index('                            Drives[lcv] = overlay;', a) + len('                            Drives[lcv] = overlay;')
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-overlay'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <stdexcept>
#include <vector>
#include <string>
int destroyed_base=0,destroyed_overlay=0,constructor_error=0;
struct DOS_Drive {virtual ~DOS_Drive(){++destroyed_base;}};
struct localDrive:DOS_Drive {const char *getBasedir(){return "base";}};
struct Overlay_Drive:localDrive {
 Overlay_Drive(const char*,const char*,int,int,int,int,int,unsigned char &error,const std::vector<std::string>&){error=constructor_error;}
 ~Overlay_Drive(){++destroyed_overlay;}
};
DOS_Drive *Drives[1];
struct {int bytes_sector=512,sectors_cluster=1,total_clusters=10,free_clusters=5,mediaid=0xf8;} oalloc;
void E_Exit(const char *s){throw std::runtime_error(s);}
void replace(){int lcv=0;unsigned char error=0;const char *overlaydir="overlay";std::vector<std::string> options;
BODY
}
int main(){
 auto *base=new localDrive;Drives[0]=base;constructor_error=2;
 bool fatal=false;try{replace();}catch(const std::runtime_error&){fatal=true;}
 assert(fatal && Drives[0]==base && destroyed_base==1 && destroyed_overlay==1);
 constructor_error=0;replace();
 assert(Drives[0]!=base && destroyed_base==2 && destroyed_overlay==1);
 delete Drives[0];assert(destroyed_base==3 && destroyed_overlay==2);
}
'''.replace('BODY',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual overlay replacement lifetime/error span ASan/UBSan PASS; not native overlay restore qualification')
