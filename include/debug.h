/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_DEBUG_H
#define DOSBOX_DEBUG_H

#include "dosbox.h"
#include <cstdint>
#include <string>
#include <vector>

#include "native_debug_execution.h"
#include "native_control_owner.h"

// Irrecoverable synchronous decoder return/exception: dispose this process.
// Never cleared by debugger stop/resume or script completion.
bool DEBUG_ContinuationUnavailable(void);
void DEBUG_SetupConsole(void);
void DEBUG_DrawScreen(void);
bool DEBUG_Breakpoint(void);
bool DEBUG_RetrySuppressedBreakpoint(void);
bool DEBUG_IntBreakpoint(uint8_t intNum);
void DEBUG_Enable(bool pressed);
void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off);
bool DEBUG_ExitLoop(void);
void DEBUG_NativeDecoderEntered(void);
// Borrow request flags for exactly one decoder invocation, not its callback.
unsigned DEBUG_EnterNativeDecoderFlags(void);
void DEBUG_LeaveNativeDecoderFlags(unsigned saved);
bool DEBUG_NativeDecoderRestoresCore(void);
bool DEBUG_IsNativeDebuggerCallback(Bits callback);
bool DEBUG_NativeExecutionBudgetExpired(void);
void DEBUG_RefreshPage(char scroll);
Bitu DEBUG_EnableDebugger(void);
// Fatal DOS-corruption diagnostic UI only: no recursive machine execution.
void DEBUG_WaitNoExecute(void);
#if C_DEBUG
uint64_t GetAddress(uint16_t seg, uint32_t offset);
int32_t DEBUG_Run(int32_t amount, bool quickexit);
#endif
#if C_DEBUG_LUA
void DEBUG_InstallLocalLuaDriver();
void DEBUG_CloseLocalLuaDriver();
void DEBUG_LocalLuaDriverRootReturned();
void DEBUG_LocalLuaDriverShellReturned();
int DEBUG_LocalLuaDriverExitStatus();
// Native fatal exit unwinds guest frames before cooperative worker disposal.
bool DEBUG_UnwindLocalLuaDriverFatal();
bool DEBUG_RunStartupLuaScriptInStoppedState(const char *script_path);
#endif
bool IsDebuggerActive(void);
bool IsDebuggerRunwatch(void);
bool IsDebuggerRunNormal(void);

#if C_DEBUG
bool DEBUG_ParseInterruptArgument(const char *argument, uint8_t &interrupt);
#if C_HEAVY_DEBUG
void DEBUG_ParseLogArgument(const char *argument, int &count);
#endif
bool DEBUG_AgentStep(bool over, bool* continued);
bool DEBUG_ExecuteSessionCommand(const char* command, NativeDebug::Attachment attachment);
NativeDebug::Attachment DEBUG_AttachNativeExecution(NativeDebug::Frontend frontend, uint64_t principal);
bool DEBUG_AdmitNativeEffect(NativeDebug::Attachment attachment);
NativeDebug::Admission DEBUG_RequestNativeExecution(NativeDebug::Request request);
bool DEBUG_ResumeNativeExecution(NativeDebug::Frontend frontend, bool watch, bool retrace, NativeDebug::Attachment attachment,
                                 const std::vector<NativeDebug::StopCondition> &conditions = {});
bool DEBUG_NativeExecutionPending(void);
// Ingress must not run during activation or its callback tail.
bool DEBUG_NativeExecutionInFlight(void);
// True only inside the post-service dispatcher for the active Phase grant.
bool DEBUG_NativePhaseExecuting(void);
struct DebuggerPhaseResult;
const DebuggerPhaseResult *DEBUG_NativePhaseResult(NativeDebug::Attachment attachment, uint64_t operation);
NativeDebug::Context DEBUG_NativeExecutionContext(void);
bool DEBUG_AdvanceNativeStateContext(NativeDebug::Attachment attachment, NativeDebug::Context expected,
                                    NativeControl::Lease held);
// Synchronous original-engine operation under the selected native stop/lease.
// A successful restore retires the supplied context before returning.
bool DEBUG_NativeStateFile(NativeDebug::Attachment attachment, NativeDebug::Context expected,
                           const std::string &path, bool load,
                           StateArchive::Effects *effects = nullptr);
bool DEBUG_AdmitNativeObservation(NativeDebug::Attachment attachment, NativeDebug::Context expected);
// Only the state job may present its exact held lease.
bool DEBUG_AdmitNativeState(NativeDebug::Attachment attachment, NativeDebug::Context expected,
                           const NativeControl::Lease *held = nullptr, bool require_stopped = true);
const NativeDebug::Result *DEBUG_NativeExecutionResult(NativeDebug::Attachment attachment);
bool DEBUG_ReleaseNativeExecutionResult(NativeDebug::Attachment attachment, uint64_t operation);
bool DEBUG_DetachNativeExecution(NativeDebug::Attachment attachment);
bool DEBUG_AgentResumeAfterTerminate(void);
uint64_t DEBUG_NativeExecutionOperation(void);
void DEBUG_AgentBeginTarget(void);
void DEBUG_AgentTargetTerminated(uint16_t psp);
bool DEBUG_AgentEndTarget(void);
bool DEBUG_AgentCanStartTarget(void);
void DEBUG_MachineOwnerDeparting(uint64_t token);
// Complete decoder callbacks after debugger request/evaluator unwind.
Bits DEBUG_DispatchNativeExecution(void);
bool DEBUG_DeferPredicateInstruction(void);
uint64_t DEBUG_AgentEntryBreakpointSequence(void);
bool DEBUG_AgentCreateExecutionBreakpoint(uint16_t seg, uint32_t off, bool once, uint64_t* handle, NativeDebug::Attachment attachment, NativeDebug::Context expected);
bool DEBUG_AgentCreateMemoryBreakpoint(uint16_t seg, uint32_t off, bool protected_mode, bool linear, uint64_t* handle, NativeDebug::Attachment attachment, NativeDebug::Context expected);
bool DEBUG_AgentDeleteBreakpoint(uint64_t handle, NativeDebug::Attachment attachment, NativeDebug::Context expected);
uint64_t DEBUG_AgentConsumeLastBreakpoint(void);
void DEBUG_AgentClearLastBreakpoint(void);
bool DEBUG_AgentBeginOutputCapture(void);
std::string DEBUG_AgentEndOutputCapture(void);
#if C_HEAVY_DEBUG
struct DEBUG_AgentTraceEvent {
    uint16_t cs = 0;
    uint32_t instruction_pointer = 0;
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    uint32_t esi = 0;
    uint32_t edi = 0;
    uint32_t ebp = 0;
    uint32_t esp = 0;
    uint16_t ds = 0;
    uint16_t es = 0;
    uint16_t fs = 0;
    uint16_t gs = 0;
    uint16_t ss = 0;
    uint32_t flags = 0;
    bool protected_mode = false;
    std::string instruction;
    std::string analysis;
};
bool DEBUG_AgentStartTrace(uint32_t instruction_count, NativeDebug::Attachment attachment, NativeDebug::Context expected);
bool DEBUG_AgentStopTrace(uint32_t* event_count, NativeDebug::Attachment attachment);
bool DEBUG_AgentTraceIsActive(void);
bool DEBUG_AgentCopyTraceEvents(std::vector<DEBUG_AgentTraceEvent>* events, NativeDebug::Attachment attachment);
#endif
#endif

extern Bitu cycle_count;
extern Bitu debugCallback;

#ifdef C_HEAVY_DEBUG
bool DEBUG_HeavyIsBreakpoint(void);
void DEBUG_HeavyWriteLogInstruction(void);
#endif

#endif
