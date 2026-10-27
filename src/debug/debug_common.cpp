/*
 *  Copyright (C) 2002-2026  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "dosbox.h"
#if C_DEBUG

#include "debug_common.h"
#include "native_control_owner.h"
NativeControl::Owner &NativeControl::owner() {
    static Owner instance;
    return instance;
}

#include "cpu.h"
#include "debug_inc.h"
#include "debug.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"

#include <ctype.h>
#include <chrono>
#include <limits>
#include <stdio.h>
#include <string.h>

static bool continuation_unavailable = false;
bool DEBUG_ContinuationUnavailable(void) { return continuation_unavailable; }

DebuggerVersion Debugger_GetVersion(void)
{
	return {"0.1.0", "dosbox-x-debugger-common"};
}

DebuggerStatus Debugger_GetStatus(void)
{
	const bool active = IsDebuggerActive();
	const bool run_watch = IsDebuggerRunwatch();
	const bool run_normal = IsDebuggerRunNormal();

	return {active, run_watch, run_normal, active && !run_watch && !run_normal};
}


static DebuggerStopEvent debugger_pending_stop_event = {};
static uint64_t debugger_stop_sequence = 0;
static bool debugger_step_in_progress = false;

static void Debugger_ClearStopEvent(DebuggerStopEvent &event)
{
	memset(&event, 0, sizeof(event));
	event.type = DebuggerStopType::None;
	event.breakpoint_owner = DebuggerBreakpointOwner::Unknown;
}

static void Debugger_CaptureStopLocation(DebuggerStopEvent &event, const uint16_t cs_value, const uint32_t ip_value)
{
	event.cs = cs_value;
	event.ip = ip_value;
}

static void Debugger_CaptureCurrentStopLocation(DebuggerStopEvent &event)
{
	const DebuggerRegisters regs = Debugger_GetRegisters();
	Debugger_CaptureStopLocation(event, regs.cs, regs.ip);
}

static void Debugger_SetStopMessage(DebuggerStopEvent &event, const char *message)
{
	event.message[0] = '\0';
	if (message != NULL)
		snprintf(event.message, sizeof(event.message), "%s", message);
}

static void Debugger_FinalizeStopEvent(DebuggerStopEvent &event, const uint32_t requested_count)
{
	if (event.type == DebuggerStopType::None)
		event.type = DebuggerStopType::Step;
	if (event.sequence == 0)
		event.sequence = ++debugger_stop_sequence;
	event.requested_count = requested_count;
	if (event.cs == 0 && event.ip == 0)
		Debugger_CaptureCurrentStopLocation(event);
}

void Debugger_RecordBreakpointStop(const uint16_t seg,
                                   const uint32_t off,
                                   const uint64_t breakpoint_id,
                                   const DebuggerBreakpointOwner breakpoint_owner,
                                   const uint64_t breakpoint_hit_count,
                                   const char *breakpoint_label)
{
#if C_DEBUG_LUA
 Debugger_ExecutionRecord(breakpoint_id,breakpoint_hit_count);
#endif
	if (!debugger_step_in_progress || debugger_pending_stop_event.type != DebuggerStopType::None)
		return;

	Debugger_ClearStopEvent(debugger_pending_stop_event);
	debugger_pending_stop_event.type = DebuggerStopType::Breakpoint;
	Debugger_CaptureStopLocation(debugger_pending_stop_event, seg, off);
	debugger_pending_stop_event.breakpoint_id = breakpoint_id;
	debugger_pending_stop_event.breakpoint_owner = breakpoint_owner;
	debugger_pending_stop_event.breakpoint_hit_count = breakpoint_hit_count;
	if (breakpoint_label != NULL)
		snprintf(debugger_pending_stop_event.breakpoint_label,
		         sizeof(debugger_pending_stop_event.breakpoint_label),
		         "%s",
		         breakpoint_label);
}

static DebuggerStopEvent Debugger_RunInstructionSlices(const uint32_t requested_count,
                                                        const bool timeout_on_limit,
                                                        DebuggerPhaseResult *phase = nullptr,
                                                        uint32_t wall_ms = 0,
                                                        bool target = false,
                                                        uint16_t target_cs = 0,
                                                        uint16_t target_ip = 0)
{
 if (debugger_step_in_progress || (DEBUG_NativeExecutionInFlight() && !DEBUG_NativePhaseExecuting()) ||
     !NativeControl::diagnosticEffectAllowed()) {
     DebuggerStopEvent denied = {};
     denied.type = DebuggerStopType::Error;
     Debugger_SetStopMessage(denied, "native execution or exclusive control lease in flight");
     return denied;
 }
 const auto started = std::chrono::steady_clock::now();
	Debugger_ClearStopEvent(debugger_pending_stop_event);

	if (continuation_unavailable) {
        debugger_pending_stop_event.type = DebuggerStopType::Error;
        Debugger_SetStopMessage(debugger_pending_stop_event, "continuation unavailable; dispose session");
        Debugger_CaptureCurrentStopLocation(debugger_pending_stop_event);
        Debugger_FinalizeStopEvent(debugger_pending_stop_event, requested_count);
        return debugger_pending_stop_event;
    }
	if (!Debugger_GetStatus().stopped) {
		debugger_pending_stop_event.type = DebuggerStopType::Error;
		Debugger_SetStopMessage(debugger_pending_stop_event, "emulator is not in a debugger-safe stopped state");
		Debugger_CaptureCurrentStopLocation(debugger_pending_stop_event);
		Debugger_FinalizeStopEvent(debugger_pending_stop_event, requested_count);
		return debugger_pending_stop_event;
	}

	debugger_step_in_progress = true;
	int32_t result = 0;
	uint32_t instructions_run = 0;
	for (; instructions_run < requested_count;) {
		const DebuggerRegisters before_regs = Debugger_GetRegisters();
        if (phase) {
            if (target && before_regs.cs == target_cs && before_regs.ip == target_ip) {
                phase->reason = "target"; break;
            }
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()-started).count() >= wall_ms) {
                phase->reason = "wall_limit"; break;
            }
        }
		if (Debugger_CheckBreakpointAt(before_regs.cs, before_regs.ip))
			break;
		try {
            result = DEBUG_Run(1, true);
        } catch (...) {
            continuation_unavailable = true;
            if (phase) phase->reason = "execution_error";
            debugger_pending_stop_event.type = DebuggerStopType::Error;
            Debugger_SetStopMessage(debugger_pending_stop_event, "decoder threw; continuation unavailable; dispose session");
            break;
        }
		// Count only decoder completion, never callback/exit dispatch.
        if (result == 0) ++instructions_run;
        // Do not service callbacks or enter nested guest loops from active Lua.
        if (result != 0 && static_cast<Bitu>(result) != debugCallback) {
            continuation_unavailable = true;
            if (phase) phase->reason = "decoder_return";
            debugger_pending_stop_event.type = DebuggerStopType::Error;
            Debugger_SetStopMessage(debugger_pending_stop_event, "decoder callback/exit; continuation unavailable; dispose session");
            break;
        }
		if (debugger_pending_stop_event.type != DebuggerStopType::None)
			break;
		const DebuggerRegisters regs = Debugger_GetRegisters();
		if (Debugger_CheckBreakpointAt(regs.cs, regs.ip))
			break;
        if (phase && target && regs.cs == target_cs && regs.ip == target_ip) {
            phase->reason = "target"; break;
        }
		if (static_cast<Bitu>(result) == debugCallback)
			break;
	}
	debugger_step_in_progress = false;
    if (phase) {
        phase->slices = instructions_run;
        if (!phase->reason) phase->reason = instructions_run >= requested_count ?
            (target ? "instruction_limit" : "step") : "event";
    }

	if (debugger_pending_stop_event.type == DebuggerStopType::None) {
		if (static_cast<Bitu>(result) == debugCallback) {
			debugger_pending_stop_event.type = DebuggerStopType::ManualStop;
			Debugger_SetStopMessage(debugger_pending_stop_event, "debugger entered without a recorded stop reason");
		} else if (timeout_on_limit && instructions_run >= requested_count) {
			debugger_pending_stop_event.type = DebuggerStopType::Timeout;
			Debugger_SetStopMessage(debugger_pending_stop_event, "instruction_limit reached");
		} else {
			debugger_pending_stop_event.type = DebuggerStopType::Step;
		}
	}
	Debugger_CaptureCurrentStopLocation(debugger_pending_stop_event);
	debugger_pending_stop_event.instruction_count = instructions_run;
	Debugger_FinalizeStopEvent(debugger_pending_stop_event, requested_count);
	return debugger_pending_stop_event;
}

DebuggerPhaseResult Debugger_RunPhase(uint32_t limit, uint32_t wall_ms,
                                    bool target, uint16_t cs, uint16_t ip)
{
    DebuggerPhaseResult phase = {};
    phase.event = Debugger_RunInstructionSlices(limit, target, &phase, wall_ms, target, cs, ip);
    return phase;
}

DebuggerStopEvent Debugger_Step(const uint32_t count)
{
	return Debugger_RunInstructionSlices(count, false);
}

DebuggerStopEvent Debugger_RunUntilEvent(const uint32_t instruction_limit)
{
	return Debugger_RunInstructionSlices(instruction_limit, true);
}

DebuggerRegisters Debugger_GetRegisters(void)
{
	DebuggerRegisters regs = {};

	regs.eax = reg_eax;
	regs.ebx = reg_ebx;
	regs.ecx = reg_ecx;
	regs.edx = reg_edx;
	regs.esi = reg_esi;
	regs.edi = reg_edi;
	regs.ebp = reg_ebp;
	regs.esp = reg_esp;
	regs.eip = reg_eip;
	regs.ip  = reg_ip;

	regs.cs = SegValue(cs);
	regs.ds = SegValue(ds);
	regs.es = SegValue(es);
	regs.fs = SegValue(fs);
	regs.gs = SegValue(gs);
	regs.ss = SegValue(ss);

	regs.flags = CPU_InspectFlags();
	regs.flag_cf = ((regs.flags & FLAG_CF) != 0);
	regs.flag_pf = ((regs.flags & FLAG_PF) != 0);
	regs.flag_af = ((regs.flags & FLAG_AF) != 0);
	regs.flag_zf = ((regs.flags & FLAG_ZF) != 0);
	regs.flag_sf = ((regs.flags & FLAG_SF) != 0);
	regs.flag_tf = ((regs.flags & FLAG_TF) != 0);
	regs.flag_if = ((regs.flags & FLAG_IF) != 0);
	regs.flag_df = ((regs.flags & FLAG_DF) != 0);
	regs.flag_of = ((regs.flags & FLAG_OF) != 0);

	regs.code_big = cpu.code.big;

	return regs;
}

static bool Debugger_RegisterNameEquals(const char *lhs, const char *rhs)
{
	if (lhs == NULL || rhs == NULL)
		return false;
	while (*lhs != '\0' && *rhs != '\0') {
		if (tolower(static_cast<unsigned char>(*lhs)) != tolower(static_cast<unsigned char>(*rhs)))
			return false;
		++lhs;
		++rhs;
	}
	return *lhs == '\0' && *rhs == '\0';
}

static bool Debugger_RegisterValueFits(const uint64_t value, const uint64_t max_value)
{
	return value <= max_value;
}

DebuggerRegisterSetFailure Debugger_SetRegister(const char *name, const uint64_t value, NativeDebug::Attachment attachment)
{
    if (!DEBUG_AdmitNativeEffect(attachment)) return DebuggerRegisterSetFailure::NotStopped;
	if (DEBUG_NativeExecutionInFlight() || !NativeControl::diagnosticEffectAllowed() || !Debugger_GetStatus().stopped)
		return DebuggerRegisterSetFailure::NotStopped;
	if (name == NULL || name[0] == '\0')
		return DebuggerRegisterSetFailure::UnknownRegister;

	if (Debugger_RegisterNameEquals(name, "eax")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_eax = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "ebx")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_ebx = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "ecx")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_ecx = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "edx")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_edx = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "esi")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_esi = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "edi")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_edi = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "ebp")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_ebp = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "esp")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_esp = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "eip")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_eip = static_cast<uint32_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "ax")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_ax = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "bx")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_bx = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "cx")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_cx = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "dx")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_dx = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "si")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_si = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "di")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_di = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "bp")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_bp = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "sp")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_sp = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "ip")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_ip = static_cast<uint16_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "al")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_al = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "ah")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_ah = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "bl")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_bl = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "bh")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_bh = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "cl")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_cl = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "ch")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_ch = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "dl")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_dl = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "dh")) {
		if (!Debugger_RegisterValueFits(value, 0xffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		reg_dh = static_cast<uint8_t>(value);
	} else if (Debugger_RegisterNameEquals(name, "cs")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		SegSet16(cs, static_cast<uint16_t>(value));
	} else if (Debugger_RegisterNameEquals(name, "ds")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		SegSet16(ds, static_cast<uint16_t>(value));
	} else if (Debugger_RegisterNameEquals(name, "es")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		SegSet16(es, static_cast<uint16_t>(value));
	} else if (Debugger_RegisterNameEquals(name, "fs")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		SegSet16(fs, static_cast<uint16_t>(value));
	} else if (Debugger_RegisterNameEquals(name, "gs")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		SegSet16(gs, static_cast<uint16_t>(value));
	} else if (Debugger_RegisterNameEquals(name, "ss")) {
		if (!Debugger_RegisterValueFits(value, 0xffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		SegSet16(ss, static_cast<uint16_t>(value));
	} else if (Debugger_RegisterNameEquals(name, "eflags")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		CPU_SetFlags(static_cast<Bitu>(value), FMASK_ALL);
	} else if (Debugger_RegisterNameEquals(name, "flags")) {
		if (!Debugger_RegisterValueFits(value, 0xffffffffu))
			return DebuggerRegisterSetFailure::InvalidValue;
		CPU_SetFlags(static_cast<Bitu>(value), FMASK_ALL & 0xffffu);
	} else if (Debugger_RegisterNameEquals(name, "cf")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(CF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "pf")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(PF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "af")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(AF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "zf")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(ZF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "sf")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(SF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "tf")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(TF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "if_") || Debugger_RegisterNameEquals(name, "if")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(IF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "df")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(DF, value != 0);
	} else if (Debugger_RegisterNameEquals(name, "of")) {
		if (value > 1)
			return DebuggerRegisterSetFailure::InvalidValue;
		SETFLAGBIT(OF, value != 0);
	} else {
		return DebuggerRegisterSetFailure::UnknownRegister;
	}

	return DebuggerRegisterSetFailure::None;
}

static const uint64_t Debugger_MemNoAddress = (~0ULL);

DebuggerAddress Debugger_ResolveAddress(const uint16_t seg, const uint32_t off)
{
	DebuggerAddress address = {};
	address.seg = seg;
	address.off = off;
	address.resolved_kind = (cpu.pmode && !(reg_flags & FLAG_VM) && seg != SegValue(cs))
	                              ? DebuggerAddressKind::Linear
	                              : DebuggerAddressKind::Physical;
	address.failure = DebuggerAddressFailure::None;

	const uint64_t resolved = GetAddress(seg, off);
	if (resolved == Debugger_MemNoAddress) {
		address.failure = DebuggerAddressFailure::InvalidSelectorOrOffset;
		return address;
	}

	address.resolved = resolved;
	address.valid = true;
	return address;
}

bool Debugger_FormatAddress(char *buffer, const size_t buffer_size, const uint16_t seg, const uint32_t off)
{
	return buffer != NULL && buffer_size >= 10 && snprintf(buffer, buffer_size, "%04X:%04X", seg, off) == 9;
}

DebuggerMemoryFailure Debugger_ReadMemory(const DebuggerAddress &address,
                                          const uint32_t length,
                                          uint8_t *buffer,
                                          const uint32_t buffer_size,
                                          uint32_t *bytes_read)
{
	if (bytes_read != NULL)
		*bytes_read = 0;

	if (!address.valid)
		return DebuggerMemoryFailure::InvalidAddress;
	if (length == 0 || length > DebuggerMemoryReadMax || buffer == NULL || buffer_size < length)
		return DebuggerMemoryFailure::InvalidLength;
	if (address.resolved > std::numeric_limits<LinearPt>::max())
		return DebuggerMemoryFailure::AddressOverflow;

	const uint64_t last = address.resolved + static_cast<uint64_t>(length - 1);
	if (last < address.resolved || last > std::numeric_limits<LinearPt>::max())
		return DebuggerMemoryFailure::AddressOverflow;

	for (uint32_t i = 0; i < length; ++i)
		buffer[i] = mem_readb(static_cast<LinearPt>(address.resolved + i));

	if (bytes_read != NULL)
		*bytes_read = length;
	return DebuggerMemoryFailure::None;
}

static bool Debugger_DisassembleIsBit32(const uint16_t seg)
{
	Descriptor desc;
	if (cpu.gdt.GetDescriptor(seg, desc))
		return desc.saved.seg.big;
	return cpu.code.big;
}

static DebuggerDisassembleFailure Debugger_FormatDisassemblyLine(DebuggerDisassemblyLine &line,
                                                                 const char *instruction,
                                                                 const Bitu drawsize)
{
	char bytes[24] = {};
	char *cursor = bytes;
	const Bitu capped_drawsize = drawsize > 10 ? 9 : drawsize;

	if (capped_drawsize > 0) {
		const uint64_t last = line.resolved + static_cast<uint64_t>(capped_drawsize - 1);
		if (last < line.resolved || last > std::numeric_limits<LinearPt>::max())
			return DebuggerDisassembleFailure::AddressOverflow;
	}

	for (Bitu i = 0; i < capped_drawsize; ++i) {
		uint8_t value = 0;
		const int written = mem_readb_checked(static_cast<LinearPt>(line.resolved + i), &value)
		                            ? snprintf(cursor, sizeof(bytes) - static_cast<size_t>(cursor - bytes), "pf")
		                            : snprintf(cursor, sizeof(bytes) - static_cast<size_t>(cursor - bytes), "%02X", value);
		if (written < 0 || static_cast<size_t>(written) >= sizeof(bytes) - static_cast<size_t>(cursor - bytes))
			return DebuggerDisassembleFailure::BufferTooSmall;
		cursor += written;
	}
	if (drawsize > 10) {
		const int written = snprintf(cursor, sizeof(bytes) - static_cast<size_t>(cursor - bytes), "..");
		if (written < 0 || static_cast<size_t>(written) >= sizeof(bytes) - static_cast<size_t>(cursor - bytes))
			return DebuggerDisassembleFailure::BufferTooSmall;
	}

	if (snprintf(line.address_text, sizeof(line.address_text), "%04X:%08X", line.seg, line.off) != 13)
		return DebuggerDisassembleFailure::BufferTooSmall;

	char padded_bytes[21] = {};
	snprintf(padded_bytes, sizeof(padded_bytes), "%-20s", bytes);
	const int written = snprintf(line.text,
	                             sizeof(line.text),
	                             "%s %s%s",
	                             line.address_text,
	                             padded_bytes,
	                             instruction != NULL ? instruction : "");
	if (written < 0 || static_cast<size_t>(written) >= sizeof(line.text))
		return DebuggerDisassembleFailure::BufferTooSmall;
	return DebuggerDisassembleFailure::None;
}

DebuggerDisassembleFailure Debugger_Disassemble(const DebuggerAddress &address,
                                                const uint32_t count,
                                                DebuggerDisassemblyLine *lines,
                                                const uint32_t lines_capacity,
                                                uint32_t *lines_written)
{
	if (lines_written != NULL)
		*lines_written = 0;

	if (!address.valid)
		return DebuggerDisassembleFailure::InvalidAddress;
	if (count == 0 || count > DebuggerDisassembleMaxCount || lines == NULL || lines_capacity < count)
		return DebuggerDisassembleFailure::InvalidCount;
	if (address.resolved > std::numeric_limits<LinearPt>::max())
		return DebuggerDisassembleFailure::AddressOverflow;

	uint16_t seg = address.seg;
	uint32_t off = address.off;

	for (uint32_t i = 0; i < count; ++i) {
		const DebuggerAddress current = Debugger_ResolveAddress(seg, off);
		if (!current.valid)
			return DebuggerDisassembleFailure::InvalidAddress;
		if (current.resolved > std::numeric_limits<LinearPt>::max())
			return DebuggerDisassembleFailure::AddressOverflow;

		char instruction[DebuggerDisassembleTextMax] = {};
		/* DasmI386 and mem_readb_checked consume the existing debugger memory
		 * path as a 32-bit guest linear address. This accepts protected-mode
		 * linear addresses returned by GetAddress while still rejecting values
		 * that cannot be represented by that path. */
		const Bitu size = DasmI386(instruction,
		                          static_cast<LinearPt>(current.resolved),
		                          off,
		                          Debugger_DisassembleIsBit32(seg));
		if (size == 0)
			return DebuggerDisassembleFailure::InstructionSizeZero;
		if (static_cast<uint64_t>(size) > std::numeric_limits<uint32_t>::max())
			return DebuggerDisassembleFailure::AddressOverflow;

		DebuggerDisassemblyLine &line = lines[i];
		memset(&line, 0, sizeof(line));
		line.seg = seg;
		line.off = off;
		line.resolved = current.resolved;
		line.size = static_cast<uint32_t>(size);

		const DebuggerDisassembleFailure format_failure = Debugger_FormatDisassemblyLine(line, instruction, size);
		if (format_failure != DebuggerDisassembleFailure::None)
			return format_failure;

		const uint64_t next_off = static_cast<uint64_t>(off) + static_cast<uint64_t>(size);
		if (next_off > std::numeric_limits<uint32_t>::max())
			return DebuggerDisassembleFailure::AddressOverflow;
		off = static_cast<uint32_t>(next_off);
		if (lines_written != NULL)
			*lines_written = i + 1;
	}

	return DebuggerDisassembleFailure::None;
}

#endif
