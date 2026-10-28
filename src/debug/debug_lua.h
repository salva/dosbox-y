/*
 *  Copyright (C) 2002-2026  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef DOSBOX_DEBUG_LUA_H
#define DOSBOX_DEBUG_LUA_H

#include "dosbox.h"
#include "native_debug_execution.h"
#include <string>

#if C_DEBUG_LUA

struct DebuggerStopEvent;
bool Debugger_LuaPredicateSelect(uint64_t id, uint16_t cs, uint32_t ip, uint64_t hits);
bool Debugger_PredicateHeld();
bool Debugger_PredicatePending();
bool Debugger_LuaPredicateChannelHealthy();
bool Debugger_LuaPredicateRequestPending();
void Debugger_PredicateResolved(bool suppress, const char *diagnostic);
struct DebuggerRegisters;
const DebuggerRegisters &Debugger_PredicateRegisters();
uint32_t Debugger_PredicateTick();
bool Debugger_PredicateTasksUnchanged();


void Debugger_LuaLivePump();
void Debugger_EntryLaunchHold();
void Debugger_EntryLoaded(const char *name,uint16_t psp,uint16_t load,uint16_t cs,uint16_t ip,const char *executable="");
void Debugger_EntryMaintain();
// Selected local driver uses the same Root arm and natural LOADNGO receipt.
// 0 waits for the matching native frame, 1 is its held receipt, -1 expired.
bool Debugger_LocalDriverArm(NativeDebug::Attachment attachment,
                             NativeDebug::Context context,const std::string &program);
int Debugger_LocalDriverEntry();
// Held native startup: shared original-engine install, no guest turn.
const char *Debugger_LocalDriverLoad(NativeDebug::Attachment attachment,
                                    NativeDebug::Context context,const std::string &archive);
namespace LuaDriver { struct SaveEffects; struct Response; }
void Debugger_LocalDriverStateReady(NativeDebug::Attachment attachment,
                                   NativeDebug::Context context,LuaDriver::Response &response);
bool Debugger_LocalDriverClosing();
void Debugger_LocalDriverSave(NativeDebug::Attachment attachment,
                             NativeDebug::Context context,const std::string &path,
                             uint32_t timeout,LuaDriver::SaveEffects &effects);
bool Debugger_LuaLiveConfigured();
bool Debugger_LuaInLiveService();
void Debugger_LiveBeginSlice();
void Debugger_LiveEndSlice();
bool Debugger_LiveStop(uint64_t token);
bool Debugger_LiveCheckpointStop(uint64_t token);
bool Debugger_LiveResume(uint64_t token);
bool Debugger_LiveEffectiveStopped();
bool Debugger_LiveOwnsStop(uint64_t token);
namespace NativeFrontier { struct Descriptor; }
bool Debugger_LocalClaimStop(NativeDebug::Attachment attachment,
                            NativeDebug::Context context,
                            const NativeFrontier::Descriptor &seam);
void Debugger_LivePhaseCompleted(const DebuggerStopEvent &event);
void Debugger_LiveError();
bool Debugger_LiveGenuineStop();
const char *Debugger_LiveStopReason();
uint64_t Debugger_LiveStopStarted();
uint64_t Debugger_LiveStopFinished();
bool Debugger_LuaRunScriptInStoppedState(const char *script_path);
bool Debugger_LuaInteractive(const char *input, bool file, NativeDebug::Attachment attachment);
bool Debugger_LuaInteractiveReset();

#endif

#endif

#if C_DEBUG_LUA
bool Debugger_LuaCheckpointOwnerMustDispose(uint64_t token);
void Debugger_LuaCheckpointOwnerDeparting(uint64_t token);
bool Debugger_LuaCheckpointPoisoned(uint64_t token);
bool Debugger_LuaCheckpointDrainPending(uint64_t token);
#endif

#if C_DEBUG && C_DEBUG_LUA
struct DebuggerPredicateReceipt {
    uint64_t sequence=0,id=0,hits=0;
    uint16_t cs=0;uint32_t ip=0;
    char disposition[16]={},diagnostic[241]={};
};
struct DebuggerPredicateSummary {
    uint64_t sequence=0,evaluated=0,suppressed=0,propagated=0,failed=0;
    bool overflow=false,last_valid=false,terminal_valid=false;
    DebuggerPredicateReceipt last,terminal;
};
const DebuggerPredicateSummary &Debugger_PredicateSummary();
#endif

#if C_DEBUG && C_DEBUG_LUA
bool Debugger_LuaPrepareRestore();
// After evaluator/resource-user unwind, retire the native job before detach.
// Refusal retains failed-cleanup/poison evidence and exclusive capacity.
bool Debugger_LuaSessionPrepareDetach();
void Debugger_PredicateResetForRestore();
#endif

#if C_DEBUG && C_DEBUG_LUA
// Native Scope departure: data-only, including exception unwind.
void Debugger_ExecutionOwnerDeparting(uint64_t token);
// Settle native state borrowers before their exact machine frame disappears.
void Debugger_StateOwnerDeparting(uint64_t token);
#endif
