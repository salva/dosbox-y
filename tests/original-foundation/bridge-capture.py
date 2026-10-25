#!/usr/bin/env python3
"""Production Bridge command-body exception cleanup; not connection authority."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'bridge-capture-regression'
work.mkdir(exist_ok=True)
text = (root/'src/debug/debug_mcp.cpp').read_text()
start = text.index('void ProcessControlCommand(')
end = text.index('\n} // namespace', start)
body = text[start:end]
abort_start = text.index('void AbortCapture()')
abort_end = text.index('std::vector<std::string> EndCapture()', abort_start)
abort_body = text[abort_start:abort_end]
source = r'''
#include <string>
#include <vector>
#include <cassert>
#include <stdexcept>
#include <mutex>
std::mutex g_capture_mutex;
bool g_capture_active=false;
size_t g_capture_discarded=0;
std::vector<std::string> g_capture_lines;
#define capture g_capture_active
''' + abort_body + r'''
struct ControlRequest { std::string id, command, payload; };
struct Failure {};
int calls=0, ends=0, responses=0;
bool fail=false, break_request=false, resume=false, accepted=true;
bool ParseControlRequest(const std::string &, ControlRequest &r, std::string &) {
 r.id="1";r.command=break_request?"BREAK":"EXEC";r.payload="CPU";return true;
}
bool IsResumeDebuggerCommand(const std::string &) {return resume;}
void SendErrorResponse(const std::string &,const std::string &) {assert(false);}
void SendResponse(const std::string &,bool,const std::vector<std::string> &) {++responses;}
void WaitForOutgoingDrain() {}
void BeginCapture() {assert(!capture);capture=true;}
std::vector<std::string> EndCapture() {assert(capture);capture=false;++ends;return {};}
bool DEBUG_ExecuteCommand(const char *) {++calls;if(fail)throw Failure{};return accepted;}
void DEBUG_EnableDebugger() {++calls;if(fail)throw Failure{};}
''' + body + r'''
int main() {
 ProcessControlCommand("request");
 assert(calls==1 && ends==1 && responses==1 && !capture);
 fail=true;
 g_capture_lines.push_back("partial");g_capture_discarded=4;
 try {ProcessControlCommand("request");assert(false);} catch(const Failure &) {}
 assert(calls==2 && ends==1 && responses==1 && !capture);
 assert(g_capture_lines.empty() && g_capture_discarded==0);
 break_request=true;
 try {ProcessControlCommand("request");assert(false);} catch(const Failure &) {}
 assert(calls==3 && ends==1 && responses==1 && !capture);
 fail=false;break_request=false;accepted=false;
 ProcessControlCommand("request");
 assert(calls==4 && ends==2 && responses==2 && !capture);
 accepted=true;ProcessControlCommand("request");
 assert(calls==5 && ends==3 && responses==3 && !capture);
}
'''
(work/'capture.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'capture.cpp'),'-o',str(work/'capture')],check=True)
subprocess.run([str(work/'capture')],check=True)
print('Bridge command capture actual-body ASan/UBSan PASS (not native/connection lifetime)')
