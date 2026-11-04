#!/usr/bin/env python3
"""CON consumer native-owner null and prefailed-stream cuts; not full role identity."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'device-consumption';work.mkdir(exist_ok=True)
t=(root/'src/dos/dos_devices.cpp').read_text();codec=t[t.index('void POD_Save_DOS_Devices('):]
source=r'''
#include <cassert>
#include <cstring>
#include <sstream>
bool dos_kernel_disabled=false;
unsigned saves=0,loads=0;
struct Device {const char *name;const char *GetName(){return name;}
void SaveState(std::ostream&){++saves;}void LoadState(std::istream&,bool){++loads;}};
Device *DOS_CON=nullptr;
'''+codec+r'''
int main(){
 Device con{"CON"};
 {
  std::istringstream in;POD_Load_DOS_Devices(in);assert(in.fail() && !loads);
  std::ostringstream out;POD_Save_DOS_Devices(out);assert(out.fail() && !saves);
 }
 DOS_CON=&con;std::istringstream bad;bad.setstate(std::ios::failbit);POD_Load_DOS_Devices(bad);assert(!loads);
 std::istringstream in;POD_Load_DOS_Devices(in);assert(in && loads==1);
 std::ostringstream out;POD_Save_DOS_Devices(out);assert(out && saves==1);
}
'''
p=work/'consumer.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('DOS device native CON traversal: null and prefailed-stream refusal passed')
