#!/usr/bin/env python3
"""Actual renderer allocation bodies with poisoned allocator, not presentation proof."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'render-allocation-regression'
work.mkdir(exist_ok=True)
def body(path,start,end):
 t=(root/path).read_text();a=t.index(start);return t[a:t.index(end,a)]
a=body('src/gui/render.cpp','bool scalerSourceCacheBufferAlloc(', '\nRender_t')
b=body('src/gui/render_scalers.cpp','void Scaler_AspectChangedLinesAlloc(', '\n#define _conc2')
s=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
struct Ignore {template<class... T> void operator()(T...) {}};
#define LOG(...) Ignore{}
unsigned char *scalerSourceCacheBuffer=nullptr;
unsigned scalerSourceCacheBufferSize=0;
uint8_t *Scaler_Aspect=nullptr;
uint16_t *Scaler_ChangedLines=nullptr;
unsigned allocations=0;bool fail=false;
void *poison(size_t n){++allocations;if(fail)return nullptr;auto p=std::malloc(n);assert(p);std::memset(p,0xa5,n);return p;}
#define malloc poison
'''+a+b+r'''
int main(){
 assert(!scalerSourceCacheBufferAlloc(0,10));
 assert(!scalerSourceCacheBufferAlloc(65537,10));
 assert(scalerSourceCacheBufferAlloc(12,3));
 for(unsigned i=0;i<scalerSourceCacheBufferSize;++i)assert(scalerSourceCacheBuffer[i]==0);
 assert(!scalerSourceCacheBufferAlloc(12,3));
 std::free(scalerSourceCacheBuffer);scalerSourceCacheBuffer=nullptr;
 fail=true;assert(!scalerSourceCacheBufferAlloc(12,3));assert(!scalerSourceCacheBufferSize);fail=false;
 unsigned before=allocations;Scaler_AspectChangedLinesAlloc(UINT_MAX);assert(allocations==before);
 Scaler_AspectChangedLinesAlloc(0);assert(allocations==before);
 Scaler_AspectChangedLinesAlloc(3);
 for(unsigned i=0;i<19;++i){assert(Scaler_Aspect[i]==8);assert(Scaler_ChangedLines[i]==((i&1)?0:8192));}
 std::free(Scaler_Aspect);std::free(Scaler_ChangedLines);
}
'''
(work/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('actual renderer allocation ASan/UBSan PASS (not output/lifetime qualification)')
