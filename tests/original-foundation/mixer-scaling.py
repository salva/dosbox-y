#!/usr/bin/env python3
"""Every 8-bit sample, actual mono/stereo scaling expressions under UBSan."""
import os
import pathlib
import re
import subprocess
import tempfile
root=pathlib.Path(__file__).resolve().parents[2]
s=(root/'src/hardware/mixer.cpp').read_text().split('inline void MixerChannel::loadCurrentSample',1)[1].split('else if (sizeof(Type) == 2)',1)[0]
expressions=re.findall(r'current\[[01]\] = (.*data\+\+.*);',s)
assert len(expressions)==2
work=pathlib.Path(tempfile.mkdtemp(prefix='mixer-scaling-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
code='#include <cstdint>\n#include <cassert>\nint main(){for(int v=0;v<256;++v)for(int sign=0;sign<2;++sign){uint8_t b=uint8_t(v);const uint8_t xr=sign?0:128;const uint8_t *data=&b;int32_t got;\n'
for e in expressions:
    code+='data=&b;got='+e+';assert(got==int32_t(int8_t(b^xr))*256);\n'
code+='}}\n'
(work/'test.cpp').write_text(code)
subprocess.run(['g++','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual mono/stereo signed/unsigned 8-bit scaling all values ASan/UBSan PASS')
