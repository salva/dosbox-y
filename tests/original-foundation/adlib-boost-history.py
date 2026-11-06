#!/usr/bin/env python3
"""Actual adaptive OPL boost and original field traversal, nonzero history."""
import os
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[2]
s=(root/'src/hardware/adlib.cpp').read_text()
functions=s.split('void adlib_boost(',1)[1].split('namespace OPL2',1)[0]
# End after the second boost function; the next owner declaration is separate.
start=s.index('void adlib_boost(')
end=s.index('namespace OPL2',start)
functions=s[start:end]
fields=['last_opl_write','single_opl_write_counter','adlib_pcm_boost','adlib_pcm_gain','adlib_pcm_dc']
for f in fields:
 assert f'WRITE_POD( &{f}, {f} );' in s and f'READ_POD( &{f}, {f} );' in s
work=Path(tempfile.mkdtemp(prefix='adlib-boost-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
code='''#include <cstdint>
#include <cassert>
#include <cstring>
#include <sstream>
using pic_tickindex_t=double;
'''+s[s.index('unsigned int single_opl_write_counter'):start]+functions+'''
int main(){
last_opl_write=123.25;single_opl_write_counter=999;adlib_pcm_boost=true;
adlib_pcm_gain=17000;adlib_pcm_dc[0]=22;adlib_pcm_dc[1]=-19;
std::stringstream stream;
'''
# Global clock normally lives near OPL_Write, not in the boost declarations.
code=code.replace('int main(){','pic_tickindex_t last_opl_write=0;\nint main(){')
for f in fields:code+=f'stream.write(reinterpret_cast<const char*>(&{f}),sizeof({f}));\n'
code+='int16_t a[]={-120,131,12,-43},b[4];std::memcpy(b,a,sizeof(a));adlib_boost(a,2,2);\n'
code+='last_opl_write=0;single_opl_write_counter=0;adlib_pcm_boost=false;adlib_pcm_gain=0;adlib_pcm_dc[0]=adlib_pcm_dc[1]=0;\n'
for f in fields:code+=f'stream.read(reinterpret_cast<char*>(&{f}),sizeof({f}));\n'
code+='assert(last_opl_write==123.25 && single_opl_write_counter==999 && adlib_pcm_boost);adlib_boost(b,2,2);assert(!std::memcmp(a,b,sizeof(a)));}\n'
(work/'test.cpp').write_text(code)
subprocess.run(['g++','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual adaptive OPL boost nonzero gain/DC and clock/count/boost continuation ASan/UBSan PASS')
