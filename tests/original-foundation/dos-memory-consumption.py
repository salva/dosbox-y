#!/usr/bin/env python3
"""DOS allocator wire consumer against its native mode setter (no runtime claim)."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'dos-memory-consumption'
work.mkdir(exist_ok=True)
text = (root / 'src/dos/dos_memory.cpp').read_text()
def function(name):
    start = text.index(name)
    begin = text.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
source = r'''
#include <cassert>
#include <cstdint>
#include <sstream>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
uint16_t memAllocStrategy=0;
'''
source += function('bool DOS_SetMemAllocStrategy(') + '\n'
source += function('void POD_Load_DOS_Memory(') + '\n'
source += r'''
int main() {
 for(unsigned i=0;i<=UINT16_MAX;++i) {
  const uint16_t value=i; std::string bytes((const char*)&value,sizeof(value));
  std::istringstream stream(bytes); memAllocStrategy=0x81;
  POD_Load_DOS_Memory(stream);
  const bool valid=(value&0x3f)<3;
  assert(bool(stream)==valid);
  assert(memAllocStrategy==(valid?value:0x81));
 }
 for(unsigned n=0;n<sizeof(uint16_t);++n) {
  std::istringstream stream(std::string(n,'\0'));memAllocStrategy=0x81;
  POD_Load_DOS_Memory(stream);assert(stream.fail() && memAllocStrategy==0x81);
 }
}
'''
path = work / 'consumer.cpp'
path.write_text(source)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=undefined', str(path), '-o', str(work/'consumer')], check=True)
subprocess.run([str(work/'consumer')], check=True)
print('DOS allocator: all 65536 native strategy modes and truncations passed')
