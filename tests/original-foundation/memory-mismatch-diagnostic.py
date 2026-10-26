#!/usr/bin/env python3
"""Exercise actual memory-mismatch refusal diagnostics without parsing untrusted integers."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'src/misc/savestates.cpp').read_text()
start = source.index('\t\t\tbuffer[length]=', source.index('unzLocateFile(zf,"Memory_Size"'))
end = source.index('\n\t\t}\n', start)
body = source[start:end].replace('goto done;', 'return;')
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'memory-mismatch-diagnostic'
work.mkdir(exist_ok=True)
code = r'''
#include <string>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <cstdarg>
bool force_load_state=false,load_err=false;
const void *source = &source;
std::string loadstate_detail_saved,loadstate_detail_current, logged;
bool loadstateconfirm(int){return false;}
int MEM_TotalPages(){return 8192;}
void LOG_MSG(const char *fmt,...){char text[8192];va_list ap;va_start(ap,fmt);vsnprintf(text,sizeof(text),fmt,ap);va_end(ap);logged=text;}
void refuse(const std::string &input){
 char buffer[4096];size_t length=input.size();assert(length<sizeof(buffer));memcpy(buffer,input.data(),length);
 BODY
}
int main(){
 for(const std::string &input:std::initializer_list<std::string>{"", "incompatible", "2147483647", "-2147483648", std::string(4095,'9')}){
  load_err=false;refuse(input);assert(load_err);
  assert(loadstate_detail_saved==(input.empty()?"(none)":input+" pages"));
  assert(loadstate_detail_current=="32 MB");
  assert(logged=="Aborted. Check your memory size: "+input+" pages");
 }
}
'''.replace('BODY', body)
(work / 'test.cpp').write_text(code)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                str(work / 'test.cpp'), '-o', str(work / 'test')], check=True)
subprocess.run([str(work / 'test')], check=True)
print('Actual memory-mismatch diagnostic ASan/UBSan PASS; not owner/resource admission')
