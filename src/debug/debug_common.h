/*
 *  Copyright (C) 2002-2026  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef DOSBOX_DEBUG_COMMON_H
#define DOSBOX_DEBUG_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include "native_debug_execution.h"

static constexpr uint32_t DebuggerMemoryReadMax = 65536;
static constexpr uint32_t DebuggerDisassembleMaxCount = 256;
static constexpr size_t DebuggerDisassembleTextMax = 200;
static constexpr size_t DebuggerBreakpointLabelMax = 64;
static constexpr size_t DebuggerStopMessageMax = 160;

enum class DebuggerAddressKind {
	Physical,
	Linear,
};

enum class DebuggerAddressFailure {
	None,
	InvalidSelectorOrOffset,
};

struct DebuggerAddress {
	uint16_t seg;
	uint32_t off;
	uint64_t resolved;
	DebuggerAddressKind resolved_kind;
	DebuggerAddressFailure failure;
	bool valid;
};

enum class DebuggerMemoryFailure {
	None,
	InvalidAddress,
	InvalidLength,
	AddressOverflow,
};

enum class DebuggerRegisterSetFailure {
	None,
	NotStopped,
	UnknownRegister,
	InvalidValue,
};

enum class DebuggerDisassembleFailure {
	None,
	InvalidAddress,
	InvalidCount,
	AddressOverflow,
	BufferTooSmall,
	InstructionSizeZero,
};

enum class DebuggerBreakpointOwner {
	Lua,
	Ncurses,
	Internal,
	Unknown,
	Agent,
};

enum class DebuggerStopType {
	None,
	Step,
	Breakpoint,
	Timeout,
	Exit,
	ManualStop,
	Error,
};

enum class DebuggerBreakpointFailure {
	None,
	InvalidAddress,
	InvalidId,
	PermissionDenied,
	UnsupportedType,
	UnsupportedOption,
	Duplicate,
	BufferTooSmall,
};

enum class DebuggerTempBreakpointResult {
	None,
	Created,
	ReusedVisible,
	DisabledCollision,
	InternalCollision,
	InvalidAddress,
};

struct DebuggerBreakpointCreateOptions {
	bool enabled;
	bool temporary;
	DebuggerBreakpointOwner owner;
	const char *label;
};

struct DebuggerBreakpointInfo {
	uint64_t id;
	uint16_t seg;
	uint32_t off;
	uint64_t resolved;
	DebuggerAddressKind resolved_kind;
	bool enabled;
	bool temporary;
	DebuggerBreakpointOwner owner;
	uint64_t hit_count;
	char label[DebuggerBreakpointLabelMax];
};

struct DebuggerStopEvent {
	DebuggerStopType type;
	uint64_t sequence;
	uint64_t instruction_count;
	uint32_t requested_count;
	uint16_t cs;
	uint32_t ip;
	uint64_t breakpoint_id;
	DebuggerBreakpointOwner breakpoint_owner;
	uint64_t breakpoint_hit_count;
	char breakpoint_label[DebuggerBreakpointLabelMax];
	char message[DebuggerStopMessageMax];
};

struct DebuggerDisassemblyLine {
	uint16_t seg;
	uint32_t off;
	uint64_t resolved;
	uint32_t size;
	char address_text[14];
	char text[DebuggerDisassembleTextMax];
};

struct DebuggerVersion {
	const char *api;
	const char *backend;
};

struct DebuggerStatus {
	bool active;
	bool run_watch;
	bool run_normal;

	/*
	 * Transitional Phase 1 status only. Future Lua support must gate script
	 * execution on an explicit debugger-safe stopped-state predicate; these
	 * fields merely expose existing debugger state without changing behavior.
	 */
	bool stopped;
};

struct DebuggerRegisters {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t esp;
	uint32_t eip;
	uint16_t ip;

	uint16_t cs;
	uint16_t ds;
	uint16_t es;
	uint16_t fs;
	uint16_t gs;
	uint16_t ss;

	uint32_t flags;
	bool flag_cf;
	bool flag_pf;
	bool flag_af;
	bool flag_zf;
	bool flag_sf;
	bool flag_tf;
	bool flag_if;
	bool flag_df;
	bool flag_of;

	bool code_big;
};

DebuggerVersion Debugger_GetVersion(void);
DebuggerStatus Debugger_GetStatus(void);
DebuggerRegisters Debugger_GetRegisters(void);
DebuggerRegisterSetFailure Debugger_SetRegister(const char *name, uint64_t value, NativeDebug::Attachment attachment = {});
DebuggerAddress Debugger_ResolveAddress(uint16_t seg, uint32_t off);
DebuggerStopEvent Debugger_Step(uint32_t count);
DebuggerStopEvent Debugger_RunUntilEvent(uint32_t instruction_limit);
struct DebuggerPhaseResult {
    DebuggerStopEvent event;
    uint32_t slices;
    const char *reason;
};
DebuggerPhaseResult Debugger_RunPhase(uint32_t limit, uint32_t wall_ms,
                                    bool target, uint16_t cs, uint16_t ip);
void Debugger_RecordBreakpointStop(uint16_t seg,
                                   uint32_t off,
                                   uint64_t breakpoint_id,
                                   DebuggerBreakpointOwner breakpoint_owner,
                                   uint64_t breakpoint_hit_count,
                                   const char *breakpoint_label);
bool Debugger_CheckBreakpointAt(uint16_t seg, uint32_t off);
bool Debugger_FormatAddress(char *buffer, size_t buffer_size, uint16_t seg, uint32_t off);
DebuggerMemoryFailure Debugger_ReadMemory(const DebuggerAddress &address,
                                          uint32_t length,
                                          uint8_t *buffer,
                                          uint32_t buffer_size,
                                          uint32_t *bytes_read);
DebuggerDisassembleFailure Debugger_Disassemble(const DebuggerAddress &address,
                                                uint32_t count,
                                                DebuggerDisassemblyLine *lines,
                                                uint32_t lines_capacity,
                                                uint32_t *lines_written);
DebuggerBreakpointFailure Debugger_BreakpointSet(const DebuggerAddress &address,
                                                 const DebuggerBreakpointCreateOptions &options,
                                                 uint64_t *id_out, NativeDebug::Attachment attachment = {});
bool Debugger_BreakpointOnce(uint64_t id, DebuggerBreakpointOwner requester, NativeDebug::Attachment attachment = {});
DebuggerBreakpointFailure Debugger_BreakpointClear(uint64_t id,
                                                   DebuggerBreakpointOwner requester,
                                                   bool *cleared_out, NativeDebug::Attachment attachment = {});
bool Debugger_TraceActive();
DebuggerBreakpointFailure Debugger_BreakpointList(DebuggerBreakpointInfo *items,
                                                  uint32_t capacity,
                                                  uint32_t *count_out,
                                                  bool include_internal);
DebuggerBreakpointFailure Debugger_BreakpointClearOwned(DebuggerBreakpointOwner owner,
                                                        uint32_t *cleared_out, NativeDebug::Attachment attachment = {});
DebuggerTempBreakpointResult Debugger_PrepareRunUntilAddress(const DebuggerAddress &address,
                                                             uint64_t *temporary_id_out,
                                                             bool *created_out, bool exclusive = false);
DebuggerBreakpointFailure Debugger_ClearInternalTemporaryBreakpoint(uint64_t id,
                                                                   bool *cleared_out);

#if C_DEBUG_LUA
struct DebuggerExecutionTarget {
 uint64_t generation = 0, owner = 0, breakpoint_id = 0;
 uint64_t start_ms = 0, end_ms = 0, deadline_ms = 0, tick = 0, hit_count = 0;
 // Host diagnostic identity, outside serializer/checkpoint state.
 uint64_t terminal_owner = 0;
 unsigned arm_owner_kind = 0, arm_owner_depth = 0;
 unsigned terminal_owner_kind = 0, terminal_owner_depth = 0;
 bool terminal_regs_valid = false, terminal_flags_valid = false;
 DebuggerRegisters terminal_regs = {};
 uint16_t cs = 0, ip = 0;
 const char *reason = "empty";
 DebuggerRegisters regs = {};
};
const DebuggerExecutionTarget &Debugger_ExecutionTarget();
const char *Debugger_ExecutionArm(uint16_t cs, uint16_t ip, uint32_t wall_ms);
const char *Debugger_ExecutionCancel(uint64_t generation);
void Debugger_ExecutionMaintain(bool channel_closed = false);
// Data-only native lifecycle and pre-match qualification; never calls Lua.
void Debugger_ExecutionOwnerDeparting(uint64_t token);
bool Debugger_ExecutionBreakpointEligible(uint64_t id);
void Debugger_ExecutionRecord(uint64_t id, uint64_t hits);
#endif

#endif
