#!/usr/bin/env python3
"""Exercise the production message-formatting fragment, not GUI lifetime."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'message-format-regression'
work.mkdir(exist_ok=True)
text = (root/'src/debug/debug_gui.cpp').read_text()
start = text.index('    va_start(msg,format);', text.index('void DEBUG_ShowMsg('))
end = text.index('\n#if C_DEBUG', start)
fragment = text[start:end]
source = r'''
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
std::string format_message(const char *format, ...) {
 char buf[512]; va_list msg; size_t len;
''' + fragment + r'''
 return std::string(buf,len);
}
int main() {
 assert(format_message("%s", "abc\n\n") == "abc");
 assert(format_message("%s", "").empty());
 assert(format_message("%d", 123) == "123");
 std::string huge(8192,'a'); huge += '\n';
 assert(format_message("%s",huge.c_str()) == std::string(509,'a'));
 assert(format_message("%0600d",1).size() == 509);
 assert(format_message("%s",std::string(509,'\n').c_str()).empty());
}
'''
(work/'format.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'format.cpp'),'-o',str(work/'format')],check=True)
subprocess.run([str(work/'format')],check=True)
print('message format actual-fragment sanitizer PASS (not GUI/lifetime qualification)')
