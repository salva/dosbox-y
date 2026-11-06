#!/usr/bin/env python3
"""Actual speaker edge producer: captured high history is not fresh-low history."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/hardware/pcspeaker.cpp').read_text()
owner=s.split('static struct {',1)[1].split('} spkr;',1)[0]
assert 'bool previous_output_level;' in owner and 'bool raw_clock_gate_enabled;' in owner
assert 'WRITE_POD( &spkr, spkr );' in s and 'READ_POD( &spkr, spkr );' in s
assert 'static bool previous_output_level' not in s and 'pit_raw_clock_gate_enabled' not in s
producer=s.split('inline static void AddDelayEntry',1)[1].split('inline static void AddPITOutput',1)[0]
work=Path(tempfile.mkdtemp(prefix='speaker-history-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
code='''#include <cassert>
using pic_tickindex_t=double;
using Bitu=unsigned;
struct MixerChannel {};
#define SPKR_ENTRIES 8192
#define LOG(a,b) ignore
#define LOG_MISC 0
#define LOG_WARN 0
void ignore(const char*) {}
struct DelayEntry {double index;bool output_level;};
static struct {'''+owner+'''} spkr;
inline static void AddDelayEntry'''+producer+'''
int main(){
spkr.previous_output_level=false;spkr.raw_clock_gate_enabled=true;
AddDelayEntry(.1,true);assert(spkr.used==1);
auto captured=spkr;
AddDelayEntry(.2,true);assert(spkr.used==1);
AddDelayEntry(.3,false);assert(spkr.used==2);
spkr={};spkr=captured;
assert(spkr.raw_clock_gate_enabled);
AddDelayEntry(.2,true);assert(spkr.used==1);
AddDelayEntry(.3,false);assert(spkr.used==2);
assert(spkr.entries[1].index==.3 && !spkr.entries[1].output_level);
spkr.used=SPKR_ENTRIES;AddDelayEntry(.4,true);
assert(spkr.used==SPKR_ENTRIES && spkr.previous_output_level);
}
'''
(work/'test.cpp').write_text(code)
subprocess.run(['g++','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual speaker high/low history, raw gate, captured continuation and full queue ASan/UBSan PASS')
