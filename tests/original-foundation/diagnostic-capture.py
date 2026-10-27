#!/usr/bin/env python3
"""Actual RPC diagnostic/capture bodies: exception unwind and paging ownership."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'diagnostic-capture-regression'
work.mkdir(exist_ok=True)
gui = (root/'src/debug/debug_gui.cpp').read_text()
capture = gui[gui.index('bool DEBUG_AgentBeginOutputCapture(void)'):gui.index('\n#endif', gui.index('bool DEBUG_AgentBeginOutputCapture(void)'))]
adapter = (root/'src/agent/debugger/debugger_adapter.cpp').read_text()
start = adapter.index('    if (!DEBUG_AgentBeginOutputCapture())', adapter.index('bool DebuggerAdapter::ExecuteDiagnosticCommand'))
body = adapter[start:adapter.index('\n#else', start)]
source = r'''
#include <cassert>
#include <string>
#include <vector>
#include <stdexcept>
int debugPageCounter=7, debugPageStopAt=19;
int agent_saved_debug_page_counter=0,agent_saved_debug_page_stop_at=0;
bool agent_output_capture_active=false;
std::string agent_output_capture;
''' + capture + r'''
int calls=0; bool fail=false,accept=true;
namespace NativeDebug { enum class Frontend { Agent }; }
static int DEBUG_AttachNativeExecution(NativeDebug::Frontend, int) {return 7;}
bool DEBUG_ExecuteSessionCommand(const char*, int attachment) {
 assert(attachment==7);
 ++calls; assert(agent_output_capture_active);
 agent_output_capture="diagnostic output";
 if(fail)throw std::runtime_error("parser failure");
 return accept;
}
bool diagnostic(const std::string &command,std::string *raw_output,std::string *error) {
''' + body + r'''
}
int main() {
 std::string output="unchanged",error;
 assert(diagnostic("CPU",&output,&error) && calls==1);
 assert(output=="diagnostic output" && !agent_output_capture_active);
 assert(debugPageCounter==7 && debugPageStopAt==19);
 assert(agent_output_capture.empty());
 debugPageCounter=11; debugPageStopAt=23;
 assert(DEBUG_AgentEndOutputCapture().empty());
 assert(debugPageCounter==11 && debugPageStopAt==23);
 fail=true;output="unchanged";
 try {diagnostic("CPU",&output,&error);assert(false);}
 catch(const std::runtime_error &e){assert(std::string(e.what())=="parser failure");}
 assert(calls==2 && output=="unchanged" && !agent_output_capture_active);
 assert(debugPageCounter==11 && debugPageStopAt==23 && agent_output_capture.empty());
 fail=false;accept=false;
 assert(!diagnostic("CPU",&output,&error) && calls==3);
 assert(error=="Debugger rejected diagnostic command" && output=="diagnostic output");
 assert(DEBUG_AgentBeginOutputCapture());
 assert(!diagnostic("CPU",&output,&error) && calls==3);
 assert(agent_output_capture_active); // rejection cannot steal existing owner
 DEBUG_AgentEndOutputCapture();
 assert(debugPageCounter==11 && debugPageStopAt==23);
}
'''
(work/'capture.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'capture.cpp'),'-o',str(work/'capture')],check=True)
subprocess.run([str(work/'capture')],check=True)
print('actual diagnostic/capture bodies unwind, no replay, output and paging ownership PASS (not native session qualification)')
