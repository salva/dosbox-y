/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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


#include "dosbox.h"
#if C_DEBUG
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ios>
#include <stdio.h>
#include <list>
#include <vector>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#if defined(C_DOSBOX_AGENT)
#include "agent/stop_conditions.h"
#endif
using namespace std;

#if defined(__GNUC__) && !defined(__clang__)
// GCC incorrectly reports an uninitialized matcher in the bundled GoogleMock
// test sources when they are compiled into this translation unit.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "../../tests/tests.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "debug.h"
#include "state_archive_effects.h"
#if defined(C_DOSBOX_AGENT)
#include "agent/agent_bridge.h"
#endif
#include "debug_common.h"
static NativeDebug::Attachment command_attachment;
static NativeDebug::Attachment instrumentation_attachment;
#include "native_control_owner.h"
#include "native_frontier.h"
#include <chrono>
#if C_DEBUG_LUA
#include "lua_driver_worker.h"
#include <thread>
#include "debug_lua.h"
#include "live_control.h"
#include "machine_service.h"
#include <assert.h>
#endif
#include "cross.h" //snprintf
#include "fpu_state.h"
#include "logging.h"
#include "bios.h"
#include "timer.h"
#include "video.h"
#include "vga.h"
#include "mapper.h"
#include "pc98_gdc.h"
#include "callback.h"
#include "inout.h"
#include "paging.h"
#include "debug_inc.h"
#include "../cpu/lazyflags.h"
#include "control.h"

#include "debug_mcp.h"

bool Clear_SYSENTER_Debug();
bool Toggle_BreakSYSEnter();
bool Toggle_BreakSYSExit();

#if !defined(OSFREE)
extern bool debugger_break_on_exec;
# if defined(C_DOSBOX_AGENT)
extern unsigned int debugger_box_depth;
# endif
#endif

/* [https://github.com/joncampbell123/dosbox-x/issues/1264] ncurses non-ASCII keys are outside ASCII range (start at octal 0400 == hex 0x100) */
static inline int ncurses_aware_toupper(int x) {
	if (x >= 0x00 && x <= 0xFF) return toupper(x);
	return x;
}

#ifdef WIN32
void WIN32_Console();
#else
#include <termios.h>
#include <unistd.h>
static struct termios consolesettings;
#endif
int old_cursor_state;

const char *egc_fgc_modes[4] = {
    "pattern",
    "background",
    "foreground",
    "(invalid)",
};
char *FormatDate(uint16_t year, uint8_t month, uint8_t day);
void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);
bool pc98_pegc_linear_framebuffer_enabled(void);

bool DEBUG_HaltOnRetrace = false;

extern bool                 dos_kernel_disabled;
extern bool                 is_paused;
extern bool                 pc98_crt_mode;
extern uint8_t              GDC_display_plane;
extern uint8_t              GDC_display_plane_pending;
extern bool                 pc98_256kb_boundary;
extern bool                         gdc_5mhz_mode;
extern bool                         enable_pc98_egc;
extern bool                         enable_pc98_grcg;
extern bool                         enable_pc98_16color;
extern bool                         enable_pc98_256color;
extern bool                         enable_pc98_256color_planar;
extern bool                         enable_pc98_188usermod;
extern bool                         GDC_vsync_interrupt;
extern bool                         pc98_graphics_hide_odd_raster_200line;
extern bool                         pc98_attr4_graphic;
extern bool                         egc_enable_enable;
extern uint8_t                      pc98_gdc_tile_counter;
extern uint8_t                      pc98_gdc_modereg;
extern egc_quad                     pc98_gdc_tiles;
extern bool                         pc98_monochrome_mode;

extern uint16_t                     pc98_egc_raw_values[8];

extern uint16_t                     a1_font_load_addr;
extern uint8_t                      a1_font_char_offset;

extern egc_quad             pc98_egc_bgcm;
extern egc_quad             pc98_egc_fgcm;

extern uint8_t                     pc98_egc_access;
extern uint8_t                     pc98_egc_srcmask[2]; /* host given (Neko: egc.srcmask) */
extern uint8_t                     pc98_egc_maskef[2]; /* effective (Neko: egc.mask2) */
extern uint8_t                     pc98_egc_mask[2]; /* host given (Neko: egc.mask) */

extern uint8_t                     pc98_egc_fgc;
extern uint8_t                     pc98_egc_lead_plane;
extern uint8_t                     pc98_egc_compare_lead;
extern uint8_t                     pc98_egc_lightsource;
extern uint8_t                     pc98_egc_shiftinput;
extern uint8_t                     pc98_egc_regload;
extern uint8_t                     pc98_egc_rop;
extern uint8_t                     pc98_egc_foreground_color;
extern uint8_t                     pc98_egc_background_color;

extern bool                        pc98_egc_shift_descend;
extern uint8_t                     pc98_egc_shift_destbit;
extern uint8_t                     pc98_egc_shift_srcbit;
extern uint16_t                    pc98_egc_shift_length;

extern unsigned char        pc98_text_first_row_scanline_start;  /* port 70h */
extern unsigned char        pc98_text_first_row_scanline_end;    /* port 72h */
extern unsigned char        pc98_text_row_scanline_blank_at;     /* port 74h */
extern unsigned char        pc98_text_row_scroll_lines;          /* port 76h */
extern unsigned char        pc98_text_row_scroll_count_start;    /* port 78h */
extern unsigned char        pc98_text_row_scroll_num_lines;      /* port 7Ah */

extern uint8_t                     pc98_16col_analog_rgb_palette_index;

extern uint8_t                     pc98_pal_vga[256*3];    /* G R B    0x0..0xFF */
extern uint8_t                     pc98_pal_analog[256*3]; /* G R B    0x0..0xF */
extern uint8_t                     pc98_pal_digital[8];    /* G R B    0x0..0x7 */

extern bool logBuffSuppressConsole;
extern bool logBuffSuppressConsoleNeedUpdate;

void DEBUG_PrintGUS();
void DEBUG_PrintRTC();

// Forwards
static void DrawCode(void);
static void DrawInput(void);
static void DEBUG_RaiseTimerIrq(void);
static void SaveMemory(const char *filename, uint16_t seg, uint32_t ofs1, uint32_t num);
static void SaveMemoryBin(const char *filename, uint16_t seg, uint32_t ofs1, uint32_t num);
static void LogDEVS(void);
static void LogMCBS(void);
static void LogGDT(void);
static void LogLDT(void);
static void LogIDT(void);
static void LogXMS(void);
#if !defined(OSFREE)
static void LogEMS(void);
#endif
static void LogFNKEY(void);
static void LogPages(char* selname);
static void LogCPUInfo(void);
static void OutputVecTable(char* filename);
static void DrawVariables(void);
static void LogDOSKernMem(void);
static void LogBIOSMem(void);

extern int debuggerrun;
int debugrunmode=0;
bool runnormal=false;
bool inhibit_int_breakpoint=false;

void DEBUG_DrawInput(void) {
    DrawInput();
}

uint8_t GetMonthDays(uint8_t month);
void DOS_AddDays(uint8_t days);
void DEBUG_BeginPagedContent(void);
void DEBUG_EndPagedContent(void);
Bitu MEM_PageMaskActive(void);
Bitu MEM_TotalPages(void);
Bitu MEM_PageMask(void);
void DEBUG_WarnDynamic(void);
int CPU_IsDynamicCore(void); // cpu.cpp 0=normal, 1=dynamic, 2=dynamic core with assembler

static void LogEMUMachine(void) {
    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("Emulator machine:");

    {
        const char *m = "?";
        const char *cardName = "";

        switch (machine) {
            case MCH_HERC:      m="Hercules";   break;
            case MCH_CGA:       m="CGA";        break;
            case MCH_OLIVETTI:  m="Olivetti M24"; break;
            case MCH_3270PC:    m="IBM 3270 PC"; break;
            case MCH_TANDY:     m="Tandy";      break;
            case MCH_PCJR:      m="PCjr";       break;
            case MCH_EGA:       m="EGA";        break;
            case MCH_VGA:       m="VGA";        break;
            case MCH_AMSTRAD:   m="Amstrad";    break;
            case MCH_PC98:      m="PC-98";      break;
            case MCH_FM_TOWNS:  m="FM Towns";   break;
            case MCH_MCGA:      m="MCGA";       break;
            case MCH_MDA:       m="MDA";        break;
            default:            break;
        }

        switch (svgaCard) {
            case SVGA_None:             cardName ="";                break;
            case SVGA_S3Trio:           cardName ="S3 Trio";         break;
            case SVGA_TsengET4K:        cardName ="Tseng ET4000";    break;
            case SVGA_TsengET3K:        cardName ="Tseng ET3000";    break;
            case SVGA_ParadisePVGA1A:   cardName ="Paradise PVGA1A"; break;
            case SVGA_ATI:              cardName ="ATI";             break;
            case SVGA_DOSBoxIG:         cardName ="DOSBox Integrated Graphics"; break;
        }

        DEBUG_ShowMsg("Machine: %s %s",m, cardName);
    }

    DEBUG_EndPagedContent();
}

static void LogEMUMem(void) {
    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("Emulator memory:");
    DEBUG_ShowMsg("A20 gate:                    %s",MEM_A20_Enabled() ? "ON" : "OFF");
    DEBUG_ShowMsg("CPU address bits:            %u",MEM_get_address_bits());
    DEBUG_ShowMsg("CPU address mask:            0x%lx",((unsigned long)MEM_PageMask() << 12UL) | 0xFFFUL);
    DEBUG_ShowMsg("CPU address mask current:    0x%lx",((unsigned long)MEM_PageMaskActive() << 12UL) | 0xFFFUL);
    DEBUG_ShowMsg("Memory reported size:        %lu bytes",(unsigned long)MEM_TotalPages() << 12UL);

    DEBUG_EndPagedContent();
}

bool XMS_Active(void);

Bitu XMS_GetTotalHandles(void);
bool XMS_GetHandleInfo(Bitu &phys_location,Bitu &size,Bitu &lockcount,bool &free,Bitu handle);

LoopHandler *old_loop = NULL;

/* see also "C operator precedence" */
enum {
	EXPR_BASE=0,
	EXPR_OR,
	EXPR_XOR,
	EXPR_AND,
	EXPR_BITSHIFT,
	EXPR_ADDSUB,
	EXPR_MULDIV
};

char* AnalyzeInstruction(char* inst, bool saveSelector);
uint32_t GetHexValue(char* const str, char* &hex,bool *parsed=NULL,int exprge=EXPR_BASE);
void SkipSpace(char*& hex);

#if 0
class DebugPageHandler : public PageHandler {
public:
	uint8_t readb(PhysPt /*addr*/) {
	}
	uint16_t readw(PhysPt /*addr*/) {
	}
	uint32_t readd(PhysPt /*addr*/) {
	}
	void writeb(PhysPt /*addr*/,uint8_t /*val*/) {
	}
	void writew(PhysPt /*addr*/,uint16_t /*val*/) {
	}
	void writed(PhysPt /*addr*/,uint32_t /*val*/) {
	}
};
#endif

typedef struct MEMFinder {
	bool usePreviousValue = false;
	uint8_t opType = 0;
	uint8_t size = 1;
	uint16_t iterations = 0;
	uint16_t seg = 0;
	uint32_t ofs = 0;
	uint32_t baseLinear = 0;
	uint32_t range = 0;
	uint32_t value = 0;
	uint32_t matches = 0;
	vector<uint8_t> tableValue;
	vector<bool> tableTruth;
}MEMFinder;

MEMFinder* MEMFINDInstance = NULL;

class DEBUG;

//DEBUG*	pDebugcom	= 0;
bool	exitLoop	= false;


// Heavy Debugging Vars for logging
#if C_HEAVY_DEBUG
static ofstream 	cpuLogFile;
static bool		cpuLog			= false;
static int		cpuLogCounter	= 0;
static int		cpuLogType		= 1;	// log detail
static bool zeroProtect = false;
bool	logHeavy	= false;
#endif



// Read-only instrumentation admission witness. Never cancels retained tracing.
bool Debugger_TraceActive() {
#if C_HEAVY_DEBUG
    return cpuLog || logHeavy;
#else
    return false;
#endif
}

static struct  {
	uint32_t eax,ebx,ecx,edx,esi,edi,ebp,esp,eip;
} oldregs;

static char curSelectorName[3] = { 0,0,0 };

static Segment oldsegs[6];
static Bitu oldflags,oldcpucpl;
DBGBlock dbg;
extern Bitu cycle_count;
static bool debugging = false;
static bool debug_running = false;
static bool check_rescroll = false;
#if defined(C_DOSBOX_AGENT)
static std::atomic<uint64_t> agent_entry_breakpoint_sequence(0);
#endif

static FPU oldfpu;
static bool warn_dynamic = false;


void VGA_DebugRedraw(void);

void VGA_DebugOverrideStart(uint32_t ofs,bool sum);
void VGA_ResetDebugOverrides(void);

bool IsDebuggerActive(void) {
    return debugging;
}

bool IsDebuggerRunwatch(void) {
    return debug_running;
}

bool IsDebuggerRunNormal(void) {
    return runnormal;
}

#if C_DEBUG_LUA
// Experimental Gate G stop: no public capability until integration gates pass.
static uint64_t Debugger_LiveHostNow();
enum class PredicateHold { None, Pending, Unsupported, Propagate, Suppress };
static PredicateHold predicate_hold = PredicateHold::None;
static bool predicate_selected = false;
static MachineService::Owner predicate_owner;
static DebuggerRegisters predicate_registers;
static uint64_t predicate_selected_id=0;
static DebuggerPredicateSummary predicate_summary;
const DebuggerPredicateSummary &Debugger_PredicateSummary(){return predicate_summary;}
static void PredicateCount(uint64_t &n){if(n==UINT64_MAX)predicate_summary.overflow=true;else ++n;}
static void PredicateReceipt(const char *disposition,bool terminal) {
    predicate_summary.last_valid=true;
    auto &r=predicate_summary.last;
    snprintf(r.disposition,sizeof(r.disposition),"%s",disposition);
    if(terminal){predicate_summary.terminal=r;predicate_summary.terminal_valid=true;}
}

static Bitu predicate_tick=0;
static LiveTaskStatus predicate_tasks;
static uint64_t predicate_hold_start=0;
static bool predicate_hold_reported=false;
static MachineService::Denial predicate_denial=MachineService::Denial::None;
const DebuggerRegisters &Debugger_PredicateRegisters() {return predicate_registers;}
uint32_t Debugger_PredicateTick() {return uint32_t(predicate_tick);}
bool Debugger_PredicateTasksUnchanged() {
    const auto now=GFX_LuaTaskStatus();
    return predicate_tasks.dispatch_sequence==now.dispatch_sequence && predicate_tasks.pending==now.pending && predicate_tasks.held==now.held &&
        predicate_tasks.guest_ms==now.guest_ms && predicate_tasks.epoch==now.epoch &&
        predicate_tasks.cleanup_generation==now.cleanup_generation &&
        predicate_tasks.cleanup_state==now.cleanup_state && predicate_tasks.lifetime==now.lifetime;
}
bool Debugger_PredicateHeld() {return predicate_hold != PredicateHold::None;}
bool Debugger_PredicatePending() {return predicate_hold == PredicateHold::Pending;}
void Debugger_PredicateResolved(bool suppress, const char *diagnostic) {
    assert(Debugger_PredicatePending());
    if(diagnostic && *diagnostic)LOG_MSG("Breakpoint predicate: %.240s",diagnostic);
    PredicateCount(predicate_summary.evaluated);
    snprintf(predicate_summary.last.diagnostic,sizeof(predicate_summary.last.diagnostic),"%s",diagnostic?diagnostic:"");
    if(diagnostic && *diagnostic)PredicateCount(predicate_summary.failed);
    predicate_hold = suppress ? PredicateHold::Suppress : PredicateHold::Propagate;
}
#endif

static void DEBUG_EnterCommonStoppedState(void)
{
	/* Shared debugger-common status transition. Both the normal ncurses
	 * activation path and the explicit -debug-lua-script startup path use this
	 * helper for the state read by Debugger_GetStatus().stopped. UI/terminal
	 * ownership remains outside this helper and stays on the ncurses path only.
	 */
	debugging = true;
	debug_running = false;
	runnormal = false;
}

static void SetColor(Bitu test) {
	if (test) {
		if (has_colors()) { wattrset(dbg.win_reg,COLOR_PAIR(PAIR_BYELLOW_BLACK));}
	} else {
		if (has_colors()) { wattrset(dbg.win_reg,0);}
	}
}

#define MAXCMDLEN 254
struct SCodeViewData {
	int     cursorPos;
	uint16_t  firstInstSize;
	uint16_t  useCS;
	uint32_t  useEIPlast, useEIPmid;
	uint32_t  useEIP;
	uint16_t  cursorSeg;
	uint32_t  cursorOfs;
	bool    ovrMode;
	char    inputStr[MAXCMDLEN+1];
	char    suspInputStr[MAXCMDLEN+1];
	int     inputPos;
} codeViewData;

static uint16_t  dataSeg;
static uint32_t  dataOfs;
static bool    showExtend = true;
static bool    showPrintable = true;

static void ClearInputLine(void) {
	codeViewData.inputStr[0] = 0;
	codeViewData.inputPos = 0;
}

// History stuff
#define MAX_HIST_BUFFER 50
static list<string> histBuff;
static list<string>::iterator histBuffPos = histBuff.end();

/***********/
/* Helpers */
/***********/

/* dest here must be a string with a minimum length of 11. */
static char* F80ToString(int regIndex, char* dest) {
#if C_FPU_X86
	snprintf(dest, 11, "%08.2Lf", reinterpret_cast<long double&>(fpu.p_regs[regIndex]));
#elif defined(HAS_LONG_DOUBLE)
	snprintf(dest, 11, "%08.2Lf", fpu.regs_80[regIndex].v);
#else
	snprintf(dest, 11, "%08.2f", fpu.regs[regIndex].v);
#endif
	
	return dest;
}

static bool F80TestUpdate(int regIndex) {
	if(fpu.sw.top != oldfpu.sw.top) { /* If the top changed then all registers rotated places, thus updated. */
		return true;
	}
	
#if C_FPU_X86
	return !(fpu.p_regs[regIndex].m1 == oldfpu.p_regs[regIndex].m1 && fpu.p_regs[regIndex].m2 == oldfpu.p_regs[regIndex].m2 && fpu.p_regs[regIndex].m3 == oldfpu.p_regs[regIndex].m3);
#elif defined(HAS_LONG_DOUBLE)
	return fpu.regs_80[regIndex].v != oldfpu.regs_80[regIndex].v; /* I'm certain that strict float equality can be used here. */
#else
	return fpu.regs[regIndex].v != oldfpu.regs[regIndex].v;
#endif
}

static const uint64_t mem_no_address = (~0ULL);

uint64_t LinMakeProt(uint16_t selector, uint32_t offset)
{
	Descriptor desc;

    if (cpu.gdt.GetDescriptor(selector,desc)) {
        if (selector >= 8 && desc.Type() != 0) {
            if ( (!desc.GetExpandDown() && offset <= desc.GetLimit()) ||
                 ( desc.GetExpandDown() && offset >  desc.GetLimit()) )
                return desc.GetBase()+(uint64_t)offset;
        }
    }

	return mem_no_address;
}

static bool Is16BitSegment(const uint16_t seg)
{
	if (cpu.pmode && !(reg_flags & FLAG_VM)) {
		if (seg == SegValue(cs))
			return !cpu.code.big;

		Descriptor desc;
		return cpu.gdt.GetDescriptor(seg, desc) ? !desc.saved.seg.big : false;
	}

	return true;
}

uint64_t GetAddress(uint16_t seg, uint32_t offset)
{
	/* In 16-bit modes, segment offsets wrap to 16 bits. This also normalizes
	 * values that came from signed arithmetic (for example, -1 -> 0xFFFF). */
	if (Is16BitSegment(seg))
		offset &= 0xffffu;

	/* For the current CS, always use the cached hidden base (SegPhys(cs)).
	 * Real x86 segment registers have a hidden descriptor cache that is only
	 * updated when a new selector is loaded. After LMSW sets CR0.PE=1 but
	 * before a far jump reloads CS, cpu.pmode is true yet SegValue(cs) still
	 * holds the previous (real-mode) value, so resolving via the GDT would
	 * read garbage. */
	if (seg == SegValue(cs)) return SegPhys(cs)+(uint64_t)offset;

	if (cpu.pmode && !(reg_flags & FLAG_VM))
		return LinMakeProt(seg,offset);

	return ((uint64_t)seg<<4u)+offset;
}

static char empty_sel[] = { ' ',' ',0 };

bool GetDescriptorInfo(char* selname, char* out1, char* out2)
{
	Bitu sel;
	Descriptor desc;

	if (strstr(selname,"cs") || strstr(selname,"CS")) sel = SegValue(cs);
	else if (strstr(selname,"ds") || strstr(selname,"DS")) sel = SegValue(ds);
	else if (strstr(selname,"es") || strstr(selname,"ES")) sel = SegValue(es);
	else if (strstr(selname,"fs") || strstr(selname,"FS")) sel = SegValue(fs);
	else if (strstr(selname,"gs") || strstr(selname,"GS")) sel = SegValue(gs);
	else if (strstr(selname,"ss") || strstr(selname,"SS")) sel = SegValue(ss);
	else {
		sel = GetHexValue(selname,selname);
		if (*selname==0) selname=empty_sel;
	}
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		switch (desc.Type()) {
			case DESC_TASK_GATE:
				sprintf(out1,"%s: s:%08lX type:%02X p",selname,(unsigned long)desc.GetSelector(),(int)desc.saved.gate.type);
				sprintf(out2,"    TaskGate   dpl : %01X %1X",desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
			case DESC_LDT:
			case DESC_286_TSS_A:
			case DESC_286_TSS_B:
			case DESC_386_TSS_A:
			case DESC_386_TSS_B:
				sprintf(out1,"%s: b:%08lX type:%02X pag",selname,(unsigned long)desc.GetBase(),(int)desc.saved.seg.type);
				sprintf(out2,"    l:%08lX dpl : %01X %1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.g);
				return true;
			case DESC_286_CALL_GATE:
			case DESC_386_CALL_GATE:
				sprintf(out1,"%s: s:%08lX type:%02X p params: %02X",selname,(unsigned long)desc.GetSelector(),desc.saved.gate.type,desc.saved.gate.paramcount);
				sprintf(out2,"    o:%08lX dpl : %01X %1X",(unsigned long)desc.GetOffset(),desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
			case DESC_286_INT_GATE:
			case DESC_286_TRAP_GATE:
			case DESC_386_INT_GATE:
			case DESC_386_TRAP_GATE:
				sprintf(out1,"%s: s:%08lX type:%02X p",selname,(unsigned long)desc.GetSelector(),desc.saved.gate.type);
				sprintf(out2,"    o:%08lX dpl : %01X %1X",(unsigned long)desc.GetOffset(),desc.saved.gate.dpl,desc.saved.gate.p);
				return true;
		}
		sprintf(out1,"%s: b:%08lX type:%02X parbg",selname,(unsigned long)desc.GetBase(),desc.saved.seg.type);
		sprintf(out2,"    l:%08lX dpl : %01X %1X%1X%1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		return true;
	} else {
		strcpy(out1,"                                  ");
		strcpy(out2,"                                  ");
	}
	return false;
}

/********************/
/* DebugVar   stuff */
/********************/

class CDebugVar
{
public:
	CDebugVar(char* _name, PhysPt _adr) { adr=_adr; safe_strncpy(name,_name,16); hasvalue = false; value = 0; };

	char*  GetName (void)                 { return name; };
	PhysPt GetAdr  (void)                 { return adr; };
	void   SetValue(bool has, uint16_t val) { hasvalue = has; value=val; };
	uint16_t GetValue(void)                 { return value; };
	bool   HasValue(void)                 { return hasvalue; };

private:
	PhysPt  adr;
	char    name[16];
	bool    hasvalue;
	uint16_t  value;

public:
	static void       InsertVariable(char* name, PhysPt adr);
	static CDebugVar* FindVar       (PhysPt pt);
	static CDebugVar* FindVar       (const std::string& name);
	static void       DeleteAll     ();
	static bool       SaveVars      (char* name);
	static bool       LoadVars      (char* name);

	static std::vector<CDebugVar*> varList;
};

std::vector<CDebugVar*> CDebugVar::varList;

static void AnnotateDirectBranch(char* line, const size_t line_size)
{
	char* mnemonic = line;
	while (*mnemonic == ' ' || *mnemonic == '\t') ++mnemonic;

	char* operand = mnemonic;
	while (isalpha(static_cast<unsigned char>(*operand))) ++operand;
	const std::string instruction(mnemonic, operand - mnemonic);
	if (instruction != "call" && instruction != "jmp" &&
	    (instruction.empty() || instruction[0] != 'j') &&
	    instruction.compare(0, 4, "loop") != 0)
		return;

	while (*operand == ' ' || *operand == '\t') ++operand;
	for (const char* qualifier : {"far ", "near ", "short "}) {
		const size_t length = strlen(qualifier);
		if (strncmp(operand, qualifier, length) == 0) {
			operand += length;
			break;
		}
	}

	char* end = operand;
	while (isxdigit(static_cast<unsigned char>(*end))) ++end;
	if (end == operand || (*end != '\0' && *end != ' ' && *end != '\t')) return;

	char* parse_end = nullptr;
	const unsigned long target = strtoul(operand, &parse_end, 16);
	if (parse_end != end || target > UINT32_MAX) return;

	CDebugVar* variable = CDebugVar::FindVar(static_cast<PhysPt>(target));
	if (!variable) return;

	const size_t used = strlen(line);
	const size_t available = used < line_size ? line_size - used : 0;
	if (available > 1)
		snprintf(line + used, available, " <%s>", variable->GetName());
}


/********************/
/* Breakpoint stuff */
/********************/

bool mustCompleteInstruction = false;
bool skipFirstInstruction = false;

enum EBreakpoint { BKPNT_UNKNOWN, BKPNT_PHYSICAL, BKPNT_INTERRUPT, BKPNT_MEMORY, BKPNT_MEMORY_PROT, BKPNT_MEMORY_LINEAR, BKPNT_MEMORY_FREEZE };

#define BPINT_ALL 0x100

class CBreakpoint
{
public:

    static bool HasIndependentAt(uint64_t id,PhysPt location) {
        for(auto bp:BPoints)
            if(bp->GetId()!=id && bp->IsActive() && bp->GetType()==BKPNT_PHYSICAL &&
               bp->GetLocation()==location)return true;
        return false;
    }
	CBreakpoint(void);
	void					SetAddress		(uint16_t seg, uint32_t off)	{ location = (PhysPt)GetAddress(seg,off); type = BKPNT_PHYSICAL; segment = seg; offset = off; };
	void					SetAddress		(PhysPt adr)				{ location = adr; type = BKPNT_PHYSICAL; };
	void					SetInt			(uint8_t _intNr, uint16_t ah, uint16_t al)	{ intNr = _intNr; ahValue = ah; alValue = al; type = BKPNT_INTERRUPT; };
	void					SetOnce			(bool _once)				{ once = _once; };
	void					SetEnabled		(bool _enabled)			{ enabled = _enabled; };
	void					SetOwner		(DebuggerBreakpointOwner _owner) { owner = _owner; };
	void					SetLabel		(const char *_label);
	void					SetType			(EBreakpoint _type)			{ type = _type; };
	void					SetValue		(uint8_t value)				{ ahValue = value; };
	void					SetOther		(uint8_t other)				{ alValue = other; };	

	bool					IsActive		(void)						{ return active; };
	bool					IsEnabled		(void)					{ return enabled; };
	void					Activate		(bool _active);

	EBreakpoint				GetType			(void)						{ return type; };
	bool					GetOnce			(void)						{ return once; };
	PhysPt					GetLocation		(void)						{ return location; };
	uint16_t					GetSegment		(void)						{ return segment; };
	uint32_t					GetOffset		(void)						{ return offset; };
	uint8_t					GetIntNr		(void)						{ return intNr; };
	uint16_t					GetValue		(void)						{ return ahValue; };
	uint16_t					GetOther		(void)						{ return alValue; };
	uint64_t					GetId			(void)						{ return id; };
	DebuggerBreakpointOwner	GetOwner		(void)						{ return owner; };
    uint64_t GetMachineOwner() const {return machine_owner;}
    void SetMachineOwner(uint64_t value) {machine_owner=value;}
	uint64_t					GetHitCount		(void)					{ return hit_count; };
	const char*				GetLabel		(void)						{ return label; };
	void					IncrementHitCount(void)				{ ++hit_count; };

	// statics
	static CBreakpoint*		AddBreakpoint		(uint16_t seg, uint32_t off, bool once, DebuggerBreakpointOwner owner = DebuggerBreakpointOwner::Unknown, bool enabled = true, const char *label = nullptr, bool after_existing = false);
	static CBreakpoint*		AddIntBreakpoint	(uint8_t intNum, uint16_t ah, uint16_t al, bool once);
	static CBreakpoint*		AddMemBreakpoint	(uint16_t seg, uint32_t off);
	static void				DeactivateBreakpoints();
	static void				ActivateBreakpoints	();
	static void				ActivateBreakpointsExceptAt(PhysPt adr);
	static bool				CheckBreakpoint		(uint16_t seg, uint32_t off);
	static bool				CheckEnabledBreakpoint(uint16_t seg, uint32_t off);
	static bool				CheckIntBreakpoint	(PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue);
	static CBreakpoint*		FindPhysBreakpoint	(uint16_t seg, uint32_t off, bool once);
	static bool				HasVisiblePermanentPhysBreakpointAt(PhysPt adr);
	static DebuggerTempBreakpointResult PrepareRunUntilAddress(const DebuggerAddress &address, uint64_t *temporary_id_out, bool *created_out, bool exclusive);
	static bool				DeleteInternalTemporaryById(uint64_t id, bool *cleared);
	static CBreakpoint*		FindOtherActiveBreakpoint(PhysPt adr, CBreakpoint* skip);
	static bool				IsBreakpoint		(uint16_t seg, uint32_t off);
	static bool				DeleteBreakpoint	(uint16_t seg, uint32_t off);
	static bool				DeleteBreakpoint		(CBreakpoint* breakpoint);
	static bool				DeleteByIndex		(uint16_t index);
	static bool				DeleteById		(uint64_t id, DebuggerBreakpointOwner requester, bool *cleared);
    static bool MarkOnce(uint64_t id, DebuggerBreakpointOwner requester);
    static void DeleteSession(uint64_t session) {
        for (auto i=BPoints.begin();i!=BPoints.end();) {
            auto *bp=*i;
            if (bp->session_owner!=session) {++i;continue;}
            bp->Activate(false);
            if (lastTriggered==bp->id) lastTriggered=0;
            i=BPoints.erase(i);delete bp;
        }
    }
	static uint32_t			DeleteOwned		(DebuggerBreakpointOwner owner);
    static void RetireMachineOwner(uint64_t token);
	static DebuggerBreakpointFailure ListVisible(DebuggerBreakpointInfo *items, uint32_t capacity, uint32_t *count_out, bool include_internal);
	static void				DeleteAll			(void);
	static void				ShowList			(void);
	static uint64_t				ConsumeLastTriggered			(void);


private:
	EBreakpoint	type;
	// Physical
	PhysPt		location;
#if !C_HEAVY_DEBUG
	uint8_t		oldData;
#endif
	uint16_t		segment;
	uint32_t		offset;
	// Int
	uint8_t		intNr;
	uint16_t		ahValue;
	uint16_t		alValue;
	// Shared
	bool		active;
	bool		once;
	bool		enabled;
	uint64_t	id;
	DebuggerBreakpointOwner owner;
    uint64_t machine_owner = 0;
    uint64_t session_owner = instrumentation_attachment.serial;
	uint64_t	hit_count;
	char		label[DebuggerBreakpointLabelMax];

	static std::list<CBreakpoint*>	BPoints;
	static uint64_t lastTriggered;
	static uint64_t next_id;
#if C_HEAVY_DEBUG
	friend bool DEBUG_HeavyIsBreakpoint(void);
#endif
};

CBreakpoint::CBreakpoint(void):type(BKPNT_UNKNOWN),location(0),
#if !C_HEAVY_DEBUG
oldData(0xCC),
#endif
segment(0),offset(0),intNr(0),ahValue(0),alValue(0),active(false),once(false),enabled(true),id(0),owner(DebuggerBreakpointOwner::Unknown),hit_count(0),label{} {
    // Zero is invalid; identities must never wrap and address reuse must not
    // make a retained adapter handle refer to another breakpoint.
    if (next_id == UINT64_MAX) E_Exit("Debugger breakpoint identity exhausted");
    id = next_id++;
}

void CBreakpoint::Activate(bool _active)
{
#if !C_HEAVY_DEBUG
	if (GetType() == BKPNT_PHYSICAL) {
		if (_active) {
			// Set 0xCC and save old value
			uint8_t data = mem_readb(location);
			if (data != 0xCC) {
				oldData = data;
				mem_writeb(location,0xCC);
			} else if (!active) {
				// Another activate breakpoint is already here.
				// Find it, and copy its oldData value
				CBreakpoint *bp = FindOtherActiveBreakpoint(location, this);

				if (!bp || bp->oldData == 0xCC) {
					// This might also happen if there is a real 0xCC instruction here
					DEBUG_ShowMsg("DEBUG: Internal error while activating breakpoint.\n");
					oldData = 0xCC;
				} else
					oldData = bp->oldData;
			};
		} else {
			if (mem_readb(location) == 0xCC) {
				if (oldData == 0xCC)
					DEBUG_ShowMsg("DEBUG: Internal error while deactivating breakpoint.\n");

				// Check if we are the last active breakpoint at this location
				bool otherActive = (FindOtherActiveBreakpoint(location, this) != nullptr);

				// If so, remove 0xCC and set old value
				if (!otherActive)
					mem_writeb(location, oldData);
			};
		}
	}
#endif
	active = _active;
}

// Statics
std::list<CBreakpoint*> CBreakpoint::BPoints;
uint64_t CBreakpoint::lastTriggered = 0;
uint64_t CBreakpoint::next_id = 1;

void CBreakpoint::SetLabel(const char *_label)
{
	label[0] = '\0';
	if (_label != nullptr)
		snprintf(label, sizeof(label), "%s", _label);
}

CBreakpoint* CBreakpoint::AddBreakpoint(uint16_t seg, uint32_t off, bool once, DebuggerBreakpointOwner owner, bool enabled, const char *label, bool after_existing)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(once);
	bp->SetOwner		(owner == DebuggerBreakpointOwner::Unknown ? (once ? DebuggerBreakpointOwner::Internal : DebuggerBreakpointOwner::Ncurses) : owner);
	bp->SetEnabled		(enabled);
	bp->SetLabel		(label);
    // Local target is lower priority than independent registrations already
    // at the PC. Their observer may suppress its candidate, then target wins.
    if(after_existing)BPoints.push_back(bp);
    else BPoints.push_front(bp);
	return bp;
}

CBreakpoint* CBreakpoint::AddIntBreakpoint(uint8_t intNum, uint16_t ah, uint16_t al, bool once)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetInt			(intNum,ah,al);
	bp->SetOnce			(once);
	bp->SetOwner		(once ? DebuggerBreakpointOwner::Internal : DebuggerBreakpointOwner::Ncurses);
	BPoints.push_front	(bp);
	return bp;
}

CBreakpoint* CBreakpoint::AddMemBreakpoint(uint16_t seg, uint32_t off)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(false);
	bp->SetOwner		(DebuggerBreakpointOwner::Ncurses);
	bp->SetType			(BKPNT_MEMORY);
	BPoints.push_front	(bp);
	return bp;
}

void CBreakpoint::ActivateBreakpoints()
{
	// activate all breakpoints
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i)
		if ((*i)->IsEnabled())
			(*i)->Activate(true);
}

void CBreakpoint::DeactivateBreakpoints()
{
	// deactivate all breakpoints
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i)
		(*i)->Activate(false);
}

void CBreakpoint::ActivateBreakpointsExceptAt(PhysPt adr)
{
	// activate all breakpoints, except those at adr
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		// Do not activate breakpoints at adr
		if (bp->GetType() == BKPNT_PHYSICAL && bp->GetLocation() == adr)
			continue;
		if (!bp->IsEnabled())
			continue;
		bp->Activate(true);
	}
}

static bool DEBUG_ObserveBuiltinAddress(uint64_t id);
#if C_DEBUG_LUA
// Called only on the native hit stack; a false verdict suppresses this hit.
static bool LocalDriverObserveHit(uint64_t id);
static void LocalDriverOwnerDeparting(uint64_t token);
#if !C_HEAVY_DEBUG
static uint64_t local_driver_suppressed_breakpoint=0;
#endif
#endif
bool CBreakpoint::CheckBreakpoint(uint16_t seg, uint32_t off)
// Checks if breakpoint is valid and should stop execution
{
	// Quick exit if there are no breakpoints
	if (BPoints.empty()) return false;

	// Search matching breakpoint
	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint *bp = (*i);
#if C_DEBUG_LUA
        if(!Debugger_ExecutionBreakpointEligible(bp->GetId()))continue;
#endif

		if ((bp->GetType() == BKPNT_PHYSICAL) && bp->IsActive() &&
		    (bp->GetLocation() == GetAddress(seg, off))) {
			bp->IncrementHitCount();
            DEBUG_ObserveBuiltinAddress(bp->GetId());
#if C_DEBUG_LUA
            if(!LocalDriverObserveHit(bp->GetId())) {
#if !C_HEAVY_DEBUG
                local_driver_suppressed_breakpoint=bp->GetId();
#endif
                continue;
            }
            // Collision inspection is read-only, before one-shot removal.
            bool collision=false;
            for(auto other:BPoints)
                if(other!=bp && Debugger_ExecutionBreakpointEligible(other->GetId()) && other->IsActive() &&
                   other->GetType()==BKPNT_PHYSICAL && other->GetLocation()==bp->GetLocation())collision=true;
            predicate_selected = !collision && bp->GetOwner()==DebuggerBreakpointOwner::Lua &&
                Debugger_LuaPredicateSelect(bp->GetId(),seg,off,bp->GetHitCount());
            if(predicate_selected) {
                predicate_selected_id=bp->GetId();predicate_tick=PIC_Ticks;predicate_tasks=GFX_LuaTaskStatus();
                PredicateCount(predicate_summary.sequence);
                predicate_summary.last=DebuggerPredicateReceipt{};
                auto &r=predicate_summary.last;r.sequence=predicate_summary.sequence;
                r.id=bp->GetId();r.hits=bp->GetHitCount();r.cs=seg;r.ip=off;
                PredicateReceipt("pending",false);
            }
#endif
			// Found
			lastTriggered = bp->GetId();
			Debugger_RecordBreakpointStop(seg,
			                             off,
			                             bp->GetId(),
			                             bp->GetOwner(),
			                             bp->GetHitCount(),
			                             bp->GetLabel());
			if (bp->GetOnce()) {
				// delete it, if it should only be used once
				(BPoints.erase)(i);
				bp->Activate(false);
				delete bp;
			} else {
				// Also look for once-only breakpoints at this address
				bp = FindPhysBreakpoint(seg, off, true);
				if (bp
#if C_DEBUG_LUA
                    && Debugger_ExecutionBreakpointEligible(bp->GetId())
#endif
                ) {
					BPoints.remove(bp);
					bp->Activate(false);
					delete bp;
				}
			}
			return true;
		}
#if C_HEAVY_DEBUG
		// Memory breakpoint support
		else if (bp->IsActive()) {
			if ((bp->GetType()==BKPNT_MEMORY) || (bp->GetType()==BKPNT_MEMORY_PROT) || (bp->GetType()==BKPNT_MEMORY_LINEAR) || (bp->GetType()==BKPNT_MEMORY_FREEZE)) {
				// Watch Protected Mode Memoryonly in pmode
				if (bp->GetType()==BKPNT_MEMORY_PROT) {
					// Check if pmode is active
					if (!cpu.pmode) return false;
					// Check if descriptor is valid
					Descriptor desc;
					if (!cpu.gdt.GetDescriptor(bp->GetSegment(),desc)) return false;
					if (desc.GetLimit()==0) return false;
				}

				Bitu address; 
				if (bp->GetType()==BKPNT_MEMORY_LINEAR) address = bp->GetOffset();
				else address = (Bitu)GetAddress(bp->GetSegment(),bp->GetOffset());
				uint8_t value=0;
				if (mem_readb_checked((PhysPt)address,&value)) return false;
				if (bp->GetValue() != value) {
					// Yup, memory value changed
                    if (bp->GetType()==BKPNT_MEMORY_FREEZE){
                        mem_writeb_checked((PhysPt)address,bp->GetValue());
                        return false;
                    }
					DEBUG_ShowMsg("DEBUG: Memory breakpoint %s: %04X:%04X - %02X -> %02X\n",(bp->GetType()==BKPNT_MEMORY_PROT)?"(Prot)":"",bp->GetSegment(),bp->GetOffset(),bp->GetValue(),value);
					bp->SetValue(value);
					bp->IncrementHitCount();
#if C_DEBUG_LUA
                    if(!LocalDriverObserveHit(bp->GetId()))continue;
#endif
					lastTriggered = bp->GetId();
					Debugger_RecordBreakpointStop(seg, off, bp->GetId(), bp->GetOwner(),
					                             bp->GetHitCount(), bp->GetLabel());
					return true;
				}
			}
		}
#endif
	}
	return false;
}

bool CBreakpoint::CheckEnabledBreakpoint(uint16_t seg, uint32_t off)
// Checks if a logical execute breakpoint is enabled and should stop execution
{
	if (BPoints.empty()) return false;

	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint *bp = (*i);
#if C_DEBUG_LUA
        if(!Debugger_ExecutionBreakpointEligible(bp->GetId()))continue;
#endif
		if ((bp->GetType() == BKPNT_PHYSICAL) && bp->IsEnabled() &&
		    (bp->GetLocation() == GetAddress(seg, off))) {
			bp->IncrementHitCount();
            DEBUG_ObserveBuiltinAddress(bp->GetId());
			lastTriggered = bp->GetId();
			Debugger_RecordBreakpointStop(seg,
			                             off,
			                             bp->GetId(),
			                             bp->GetOwner(),
			                             bp->GetHitCount(),
			                             bp->GetLabel());
			if (bp->GetOnce()) {
				(BPoints.erase)(i);
				bp->Activate(false);
				delete bp;
			}
			return true;
		}
	}
	return false;
}

bool CBreakpoint::CheckIntBreakpoint(PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue)
// Checks if interrupt breakpoint is valid and should stop execution
{
	if (BPoints.empty()) return false;

    // unused
    (void)adr;

	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if ((bp->GetType()==BKPNT_INTERRUPT) && bp->IsActive() && (bp->GetIntNr()==intNr)) {
			if (((bp->GetValue()==BPINT_ALL) || (bp->GetValue()==ahValue)) && ((bp->GetOther()==BPINT_ALL) || (bp->GetOther()==alValue))) {
				// Ignore it once ?
				bp->IncrementHitCount();
				// Freeze the receipt before a once-only interrupt breakpoint is deleted.
				lastTriggered = bp->GetId();
				Debugger_RecordBreakpointStop(SegValue(cs), reg_eip, bp->GetId(),
				                             bp->GetOwner(), bp->GetHitCount(), bp->GetLabel());
				if (bp->GetOnce()) {
					// delete it, if it should only be used once
					(BPoints.erase)(i);
					bp->Activate(false);
					delete bp;
				}
				return true;
			}
		}
	}
	return false;
}

void CBreakpoint::DeleteAll()
{
    // Activate(false) searches live peers sharing the patched instruction.
    // Remove each list entry before freeing it so subsequent searches cannot
    // traverse retired objects, and the last peer restores the original byte.
    while (!BPoints.empty()) {
        CBreakpoint* bp = BPoints.front();
        BPoints.pop_front();
        bp->Activate(false);
        delete bp;
    }
	lastTriggered = 0;
}


bool CBreakpoint::DeleteByIndex(uint16_t index)
{
	// Search matching breakpoint
	int nr = 0;
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		if (nr==index) {
			bp = (*i);
			if (lastTriggered == bp->GetId()) lastTriggered = 0;
			(BPoints.erase)(i);
			bp->Activate(false);
			delete bp;
			return true;
		}
		nr++;
	}
	return false;
}

CBreakpoint* CBreakpoint::FindPhysBreakpoint(uint16_t seg, uint32_t off, bool once)
{
	if (BPoints.empty()) return nullptr;
#if !C_HEAVY_DEBUG
	PhysPt adr = GetAddress(seg, off);
#endif
	// Search for matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		bp = (*i);
#if C_HEAVY_DEBUG
		// Heavy debugging breakpoints are triggered by matching seg:off
		bool atLocation = bp->GetSegment() == seg && bp->GetOffset() == off;
#else
		// Normal debugging breakpoints are triggered at an address
		bool atLocation = bp->GetLocation() == adr;
#endif

		if (bp->GetType() == BKPNT_PHYSICAL && atLocation && bp->GetOnce() == once)
			return bp;
	}

	return nullptr;
}

bool CBreakpoint::HasVisiblePermanentPhysBreakpointAt(PhysPt adr)
{
	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp->GetType() == BKPNT_PHYSICAL && bp->GetLocation() == adr && !bp->GetOnce() &&
		    bp->GetOwner() != DebuggerBreakpointOwner::Internal)
			return true;
	}
	return false;
}

DebuggerTempBreakpointResult CBreakpoint::PrepareRunUntilAddress(const DebuggerAddress &address,
                                                                  uint64_t *temporary_id_out,
                                                                  bool *created_out, bool exclusive)
{
	if (temporary_id_out != nullptr)
		*temporary_id_out = 0;
	if (created_out != nullptr)
		*created_out = false;
	if (!address.valid || address.off > 0xffff)
		return DebuggerTempBreakpointResult::InvalidAddress;

	const PhysPt adr = static_cast<PhysPt>(address.resolved);
	bool saw_visible_enabled = false;
	bool saw_visible_disabled = false;
	bool saw_internal = false;

	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp->GetType() != BKPNT_PHYSICAL || bp->GetLocation() != adr)
			continue;
		if (bp->GetOnce() || bp->GetOwner() == DebuggerBreakpointOwner::Internal) {
			saw_internal = true;
			continue;
		}
		if (bp->IsEnabled())
			saw_visible_enabled = true;
		else
			saw_visible_disabled = true;
	}

	if (saw_internal)
		return DebuggerTempBreakpointResult::InternalCollision;
	if (saw_visible_disabled)
		return DebuggerTempBreakpointResult::DisabledCollision;
	if (saw_visible_enabled && exclusive)
        return DebuggerTempBreakpointResult::InternalCollision;
	if (saw_visible_enabled)
		return DebuggerTempBreakpointResult::ReusedVisible;

	CBreakpoint* bp = AddBreakpoint(address.seg,
	                               address.off,
	                               true,
	                               DebuggerBreakpointOwner::Internal,
	                               true,
	                               "lua-run-until-addr");
	if (bp == nullptr)
		return DebuggerTempBreakpointResult::InvalidAddress;
	if (temporary_id_out != nullptr)
		*temporary_id_out = bp->GetId();
	if (created_out != nullptr)
		*created_out = true;
	return DebuggerTempBreakpointResult::Created;
}

bool CBreakpoint::DeleteInternalTemporaryById(uint64_t id, bool *cleared)
{
	if (cleared != nullptr)
		*cleared = false;
	if (id == 0)
		return false;

	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp->GetId() != id)
			continue;
		if (bp->GetOwner() != DebuggerBreakpointOwner::Internal || !bp->GetOnce())
			return false;
		if (lastTriggered == id) lastTriggered = 0;
		(BPoints.erase)(i);
		bp->Activate(false);
		delete bp;
		if (cleared != nullptr)
			*cleared = true;
		return true;
	}
	return true;
}

CBreakpoint* CBreakpoint::FindOtherActiveBreakpoint(PhysPt adr, CBreakpoint* skip)
{
	std::list<CBreakpoint*>::iterator i;
	for (i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp != skip && bp->GetType() == BKPNT_PHYSICAL && bp->GetLocation() == adr && bp->IsActive())
			return bp;
	}
	return nullptr;
}

// is there a permanent breakpoint at address ?
bool CBreakpoint::IsBreakpoint(uint16_t seg, uint32_t off)
{
	return FindPhysBreakpoint(seg, off, false) != nullptr;
}

bool CBreakpoint::DeleteBreakpoint(uint16_t seg, uint32_t off)
{
	return DeleteBreakpoint(FindPhysBreakpoint(seg, off, false));
}

bool CBreakpoint::DeleteBreakpoint(CBreakpoint* breakpoint)
{
	if (breakpoint == nullptr)
		return false;
	for (std::list<CBreakpoint*>::iterator it = BPoints.begin(); it != BPoints.end(); ++it) {
		if (*it != breakpoint)
			continue;
		BPoints.erase(it);
		breakpoint->Activate(false);
		if (lastTriggered == breakpoint->GetId())
			lastTriggered = 0;
		delete breakpoint;
		return true;
	}
	return false;
}

#if defined(C_DOSBOX_AGENT)
uint64_t CBreakpoint::ConsumeLastTriggered(void)
{
	const uint64_t result = lastTriggered;
	lastTriggered = 0;
	return result;
}
#endif // C_DOSBOX_AGENT

bool CBreakpoint::DeleteById(uint64_t id, DebuggerBreakpointOwner requester, bool *cleared)
{
	if (cleared != nullptr)
		*cleared = false;
	if (id == 0)
		return false;

	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp->GetId() != id)
			continue;
		if ((requester == DebuggerBreakpointOwner::Lua || requester == DebuggerBreakpointOwner::Agent) &&
		    bp->GetOwner() != requester)
			return false;
		if (lastTriggered == id) lastTriggered = 0;
		(BPoints.erase)(i);
		bp->Activate(false);
		delete bp;
		if (cleared != nullptr)
			*cleared = true;
		return true;
	}
	return true;
}

void CBreakpoint::RetireMachineOwner(uint64_t token)
{
    if (!token) return;
    for (auto i=BPoints.begin(); i!=BPoints.end();) {
        CBreakpoint *bp=*i;
        if (bp->GetOwner()!=DebuggerBreakpointOwner::Agent ||
            bp->GetMachineOwner()!=token) {++i;continue;}
        if (lastTriggered==bp->GetId()) lastTriggered=0;
        i=BPoints.erase(i);
        bp->Activate(false);
        delete bp;
    }
}

static void DEBUG_ExecutionOwnerDeparting(uint64_t token);
void DEBUG_AgentTraceOwnerDeparting(uint64_t token);
void DEBUG_MachineOwnerDeparting(uint64_t token)
{
    // Native resources retire on exact machine unwind, irrespective of the
    // structured-RPC projection (including Failed). Never pump the server here.
    CBreakpoint::RetireMachineOwner(token);
    DEBUG_ExecutionOwnerDeparting(token);
#if C_HEAVY_DEBUG && defined(C_DOSBOX_AGENT)
    DEBUG_AgentTraceOwnerDeparting(token);
#endif
#if C_DEBUG_LUA
    Debugger_ExecutionOwnerDeparting(token);
#endif
}

uint32_t CBreakpoint::DeleteOwned(DebuggerBreakpointOwner owner)
{
	uint32_t cleared = 0;
	for (auto i = BPoints.begin(); i != BPoints.end();) {
		CBreakpoint* bp = (*i);
		if (bp->GetOwner() != owner) {
			++i;
			continue;
		}
		if (lastTriggered == bp->GetId()) lastTriggered = 0;
		i = (BPoints.erase)(i);
		bp->Activate(false);
		delete bp;
		++cleared;
	}
	return cleared;
}

DebuggerBreakpointFailure CBreakpoint::ListVisible(DebuggerBreakpointInfo *items, uint32_t capacity, uint32_t *count_out, bool include_internal)
{
	if (count_out != nullptr)
		*count_out = 0;
	if (items == nullptr && capacity != 0)
		return DebuggerBreakpointFailure::BufferTooSmall;

	uint32_t count = 0;
	for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp->GetType() != BKPNT_PHYSICAL)
			continue;
		if (!include_internal && bp->GetOwner() == DebuggerBreakpointOwner::Internal)
			continue;
		if (!include_internal && bp->GetOnce())
			continue;
		if (items != nullptr) {
			if (count >= capacity)
				return DebuggerBreakpointFailure::BufferTooSmall;

			DebuggerBreakpointInfo &info = items[count];
			memset(&info, 0, sizeof(info));
			info.id = bp->GetId();
			info.seg = bp->GetSegment();
			info.off = bp->GetOffset();
			info.resolved = bp->GetLocation();
			info.resolved_kind = DebuggerAddressKind::Physical;
			info.enabled = bp->IsEnabled();
			info.temporary = bp->GetOnce();
			info.owner = bp->GetOwner();
			info.hit_count = bp->GetHitCount();
			if (bp->GetLabel() != nullptr && bp->GetLabel()[0] != '\0')
				snprintf(info.label, sizeof(info.label), "%s", bp->GetLabel());
		}
		++count;
	}

	if (count_out != nullptr)
		*count_out = count;
	return DebuggerBreakpointFailure::None;
}

DebuggerBreakpointFailure Debugger_BreakpointSet(const DebuggerAddress &address,
                                                 const DebuggerBreakpointCreateOptions &options,
                                                 uint64_t *id_out, NativeDebug::Attachment attachment)
{
    if (id_out != nullptr) *id_out = 0;
    if (!DEBUG_AdmitNativeEffect(attachment)) return DebuggerBreakpointFailure::PermissionDenied;
	if (!address.valid)
		return DebuggerBreakpointFailure::InvalidAddress;
	if (options.temporary)
		return DebuggerBreakpointFailure::UnsupportedOption;
	if (address.off > 0xffff)
		return DebuggerBreakpointFailure::InvalidAddress;
	if (options.owner == DebuggerBreakpointOwner::Lua &&
	    CBreakpoint::HasVisiblePermanentPhysBreakpointAt(static_cast<PhysPt>(address.resolved)))
		return DebuggerBreakpointFailure::Duplicate;

	CBreakpoint* bp = CBreakpoint::AddBreakpoint(address.seg,
	                                           address.off,
	                                           false,
	                                           options.owner,
	                                           options.enabled,
	                                           options.label);
	if (bp == nullptr)
		return DebuggerBreakpointFailure::InvalidAddress;
	if (id_out != nullptr)
		*id_out = bp->GetId();
	return DebuggerBreakpointFailure::None;
}

bool CBreakpoint::MarkOnce(uint64_t id, DebuggerBreakpointOwner requester)
{
    for(auto bp:BPoints) {
        if(bp->GetId()!=id)continue;
        if(bp->GetOwner()!=requester)return false;
        bp->SetOnce(true);return true;
    }
    return false;
}

bool Debugger_BreakpointOnce(uint64_t id, DebuggerBreakpointOwner requester, NativeDebug::Attachment attachment)
{
    if (!DEBUG_AdmitNativeEffect(attachment)) return false;
    return CBreakpoint::MarkOnce(id,requester);
}

DebuggerBreakpointFailure Debugger_BreakpointClear(uint64_t id,
                                                   DebuggerBreakpointOwner requester,
                                                   bool *cleared_out, NativeDebug::Attachment attachment)
{
    if (cleared_out != nullptr) *cleared_out = false;
    if (!DEBUG_AdmitNativeEffect(attachment)) return DebuggerBreakpointFailure::PermissionDenied;
	if (id == 0)
		return DebuggerBreakpointFailure::InvalidId;
	if (!CBreakpoint::DeleteById(id, requester, cleared_out))
		return DebuggerBreakpointFailure::PermissionDenied;
	return DebuggerBreakpointFailure::None;
}

DebuggerBreakpointFailure Debugger_BreakpointList(DebuggerBreakpointInfo *items,
                                                  uint32_t capacity,
                                                  uint32_t *count_out,
                                                  bool include_internal)
{
	return CBreakpoint::ListVisible(items, capacity, count_out, include_internal);
}

DebuggerBreakpointFailure Debugger_BreakpointClearOwned(DebuggerBreakpointOwner owner,
                                                        uint32_t *cleared_out, NativeDebug::Attachment attachment)
{
    if (cleared_out != nullptr) *cleared_out = 0;
    if (!DEBUG_AdmitNativeEffect(attachment)) return DebuggerBreakpointFailure::PermissionDenied;
	const uint32_t cleared = CBreakpoint::DeleteOwned(owner);
	if (cleared_out != nullptr)
		*cleared_out = cleared;
	return DebuggerBreakpointFailure::None;
}

DebuggerTempBreakpointResult Debugger_PrepareRunUntilAddress(const DebuggerAddress &address,
                                                             uint64_t *temporary_id_out,
                                                             bool *created_out, bool exclusive)
{
	return CBreakpoint::PrepareRunUntilAddress(address, temporary_id_out, created_out, exclusive);
}

DebuggerBreakpointFailure Debugger_ClearInternalTemporaryBreakpoint(uint64_t id,
                                                                   bool *cleared_out)
{
	if (cleared_out != nullptr)
		*cleared_out = false;
	if (id == 0)
		return DebuggerBreakpointFailure::InvalidId;
	if (!CBreakpoint::DeleteInternalTemporaryById(id, cleared_out))
		return DebuggerBreakpointFailure::PermissionDenied;
	return DebuggerBreakpointFailure::None;
}


void CBreakpoint::ShowList(void)
{
	// iterate list
	int nr = 0;
	std::list<CBreakpoint*>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		CBreakpoint* bp = (*i);
		if (bp->GetType()==BKPNT_PHYSICAL) {
			DEBUG_ShowMsg("%02X. BP %04X:%04X\n",nr,bp->GetSegment(),bp->GetOffset());
		} else if (bp->GetType()==BKPNT_INTERRUPT) {
			if (bp->GetValue()==BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X\n",nr,bp->GetIntNr());
			else if (bp->GetOther()==BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X\n",nr,bp->GetIntNr(),bp->GetValue());
			else DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X AL=%02X\n",nr,bp->GetIntNr(),bp->GetValue(),bp->GetOther());
		} else if (bp->GetType()==BKPNT_MEMORY) {
			DEBUG_ShowMsg("%02X. BPMEM %04X:%04X (%02X)\n",nr,bp->GetSegment(),bp->GetOffset(),bp->GetValue());
		} else if (bp->GetType()==BKPNT_MEMORY_PROT) {
			DEBUG_ShowMsg("%02X. BPPM %04X:%08X (%02X)\n",nr,bp->GetSegment(),bp->GetOffset(),bp->GetValue());
		} else if (bp->GetType()==BKPNT_MEMORY_LINEAR ) {
			DEBUG_ShowMsg("%02X. BPLM %08X (%02X)\n",nr,bp->GetOffset(),bp->GetValue());
        } else if (bp->GetType()==BKPNT_MEMORY_FREEZE ) {
	        DEBUG_ShowMsg("%02X. FM %08X (%02X)\n",nr,bp->GetOffset(),bp->GetValue());
		}
		nr++;
	}
}

bool DEBUG_Breakpoint(void)
{
	if (inhibit_int_breakpoint) return false; /* or else stepping over INT 21h when BPINT 21h does nothing */
	/* First get the physical address and check for a set Breakpoint */
	if (!CBreakpoint::CheckBreakpoint(SegValue(cs),reg_eip)) return false;
	// Found. Breakpoint is valid
//	PhysPt where=(PhysPt)GetAddress(SegValue(cs),reg_eip);
	CBreakpoint::DeactivateBreakpoints();	// Deactivate all breakpoints
	return true;
}

bool DEBUG_RetrySuppressedBreakpoint(void) {
#if C_DEBUG_LUA && !C_HEAVY_DEBUG
    if(local_driver_suppressed_breakpoint) {
        CBreakpoint::DeactivateBreakpoints();
        CPU_CycleLeft+=CPU_Cycles;CPU_Cycles=0;
        return true;
    }
#endif
    return false;
}
bool Debugger_CheckBreakpointAt(uint16_t seg, uint32_t off)
{
	return CBreakpoint::CheckEnabledBreakpoint(seg, off);
}

bool DEBUG_IntBreakpoint(uint8_t intNum)
{
	if (inhibit_int_breakpoint) return false; /* or else stepping over INT 21h when BPINT 21h does nothing */
	/* First get the physical address and check for a set Breakpoint */
	PhysPt where=(PhysPt)GetAddress(SegValue(cs),reg_eip);
	if (!CBreakpoint::CheckIntBreakpoint(where,intNum,reg_ah,reg_al)) return false;
	// Found. Breakpoint is valid
	CBreakpoint::DeactivateBreakpoints();	// Deactivate all breakpoints
	return true;
}

// The sole native execution slot owns only the temporary breakpoint it creates.
// A visible breakpoint reused by Over is never removed by operation cleanup.
static uint64_t native_execution_breakpoint=0;
static std::vector<uint64_t> native_condition_breakpoints;
static bool native_condition_address_hit=false;
static bool DEBUG_EvaluateNativeConditions();
static void DEBUG_NativeConditionFrontier(void *,const NativeFrontier::Descriptor &);
#if C_DEBUG_LUA
static void LocalDriverFrontier(void *,const NativeFrontier::Descriptor &);
#endif
static bool StepOver(uint16_t *target_cs=nullptr,uint32_t *target_ip=nullptr)
{
	exitLoop = false;
	PhysPt start=(PhysPt)GetAddress(SegValue(cs),reg_eip);
	char dline[200];Bitu size;
	size=DasmI386(dline, start, reg_eip, cpu.code.big);

	if (strstr(dline,"call") || strstr(dline,"int") || strstr(dline,"loop") || strstr(dline,"rep")) {
        if(target_cs)*target_cs=SegValue(cs);
        if(target_ip)*target_ip=uint32_t(reg_eip+size);
		// Don't add a temporary breakpoint if there's already one here
		if (!CBreakpoint::FindPhysBreakpoint(SegValue(cs), (uint32_t)(reg_eip+size), true))
			native_execution_breakpoint=CBreakpoint::AddBreakpoint(SegValue(cs),(uint32_t)(reg_eip+size), true)->GetId();
		debugging=false;

        logBuffSuppressConsole = false;
        if (logBuffSuppressConsoleNeedUpdate) {
            logBuffSuppressConsoleNeedUpdate = false;
            DEBUG_RefreshPage(0);
        }

		DrawCode();
		mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);
		return true;
	} 
	return false;
}

#if defined(C_DOSBOX_AGENT)
void DrawRegistersUpdateOld(void);
int32_t DEBUG_Run(int32_t amount,bool quickexit);
bool ParseCommand(char* str);

bool DEBUG_AgentStep(bool over, bool* continued)
{
	if (continued == nullptr || !debugging || debug_running)
		return false;

	if (over) {
        NativeDebug::Request request;
        request.frontend=NativeDebug::Frontend::Agent;
        request.attachment=DEBUG_AttachNativeExecution(request.frontend,0);
        request.expected=DEBUG_NativeExecutionContext();
        request.over=true;
        if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted) return false;
        DEBUG_AgentClearLastBreakpoint();
        DrawRegistersUpdateOld();
		*continued = true;
		return true;
	}

	NativeDebug::Request request;
	request.frontend=NativeDebug::Frontend::Agent;
        request.attachment=DEBUG_AttachNativeExecution(request.frontend,0);
	request.expected=DEBUG_NativeExecutionContext();
	if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted) return false;
	DEBUG_AgentClearLastBreakpoint();
	DrawRegistersUpdateOld();
	exitLoop = false;
	*continued = true;
	return true;
}

bool DEBUG_AgentResumeAfterTerminate(void)
{
	if (!debugging || debug_running)
		return true;

	// RUN admits a finite first decoder call before returning to the normal
	// loop. TerminateTarget has already restored CS:IP to the
	// callback stop trampoline that must be observed by the enclosing
	// CALLBACK_RunRealInt invocation. Stepping here consumes that trampoline
	// inside the debugger and strands the synchronous DEBUGBOX command frame.
	DrawRegistersUpdateOld();
	debug_running = false;
	debugging = false;
	DrawCode();
	DrawInput();
	logBuffSuppressConsole = false;
	if (logBuffSuppressConsoleNeedUpdate) {
		logBuffSuppressConsoleNeedUpdate = false;
		DEBUG_RefreshPage(0);
	}
	CBreakpoint::ActivateBreakpoints();
	mainMenu.get_item("debugger_rundebug").check(false).refresh_item(mainMenu);
	mainMenu.get_item("debugger_runnormal").check(true).refresh_item(mainMenu);
	mainMenu.get_item("debugger_runwatch").check(false).refresh_item(mainMenu);
	DOSBOX_SetNormalLoop();
	GFX_SetTitle(-1,-1,-1,is_paused);
	return true;
}

bool DEBUG_AgentCanStartTarget(void)
{
#if !defined(OSFREE)
    return !debugging && !debug_running && !debugger_break_on_exec && debugger_box_depth == 0;
#else
	return false;
#endif
}

uint64_t DEBUG_AgentEntryBreakpointSequence(void)
{
	return agent_entry_breakpoint_sequence.load(std::memory_order_relaxed);
}

bool DEBUG_AgentCreateExecutionBreakpoint(uint16_t seg, uint32_t off, bool once, uint64_t* handle,
                                         NativeDebug::Attachment attachment, NativeDebug::Context expected)
{
    if (handle) *handle=0;
    if (!DEBUG_AdmitNativeObservation(attachment,expected) || !DEBUG_AdmitNativeEffect(attachment)) return false;
	if (handle == nullptr || GetAddress(seg,off) == mem_no_address)
		return false;
	CBreakpoint* breakpoint = CBreakpoint::AddBreakpoint(seg,off,once,DebuggerBreakpointOwner::Agent);
    if (breakpoint) breakpoint->SetMachineOwner(DOSBOX_MachineServiceOwner().token);
	*handle = breakpoint ? breakpoint->GetId() : 0;
	return breakpoint != nullptr;
}

static bool CreateMemoryBreakpoint(uint16_t seg,
                                       uint32_t off,
                                       bool protected_mode,
                                       bool linear,
                                       uint64_t* handle,
                                       NativeDebug::Attachment attachment, NativeDebug::Context expected,
                                       DebuggerBreakpointOwner frontend)
{
#if C_HEAVY_DEBUG
    if (handle) *handle=0;
    if (!DEBUG_AdmitNativeObservation(attachment,expected) || !DEBUG_AdmitNativeEffect(attachment)) return false;
	if (handle == nullptr || (protected_mode && linear))
		return false;
	const uint64_t address = linear ? static_cast<uint64_t>(off) : GetAddress(seg,off);
	if (address == mem_no_address)
		return false;
	uint8_t value = 0;
	if (mem_readb_checked(static_cast<PhysPt>(address), &value))
		return false;
	CBreakpoint* breakpoint = CBreakpoint::AddMemBreakpoint(seg,off);
	if (breakpoint == nullptr)
		return false;
	if (protected_mode)
		breakpoint->SetType(BKPNT_MEMORY_PROT);
	else if (linear)
		breakpoint->SetType(BKPNT_MEMORY_LINEAR);
	breakpoint->SetValue(value);
	breakpoint->SetOwner(frontend);
    breakpoint->SetMachineOwner(DOSBOX_MachineServiceOwner().token);
	*handle = breakpoint->GetId();
	return true;
#else
	(void)seg;
	(void)off;
	(void)protected_mode;
	(void)linear;
	(void)handle;
    (void)attachment;
    (void)expected;
    (void)frontend;
	return false;
#endif
}

bool DEBUG_AgentCreateMemoryBreakpoint(uint16_t seg, uint32_t off,
        bool protected_mode, bool linear, uint64_t *handle,
        NativeDebug::Attachment attachment, NativeDebug::Context expected) {
    return CreateMemoryBreakpoint(seg,off,protected_mode,linear,handle,
                                  attachment,expected,DebuggerBreakpointOwner::Agent);
}

bool DEBUG_AgentDeleteBreakpoint(uint64_t handle, NativeDebug::Attachment attachment, NativeDebug::Context expected)
{
    if (!DEBUG_AdmitNativeObservation(attachment,expected) || !DEBUG_AdmitNativeEffect(attachment)) return false;
	bool cleared = false;
	return CBreakpoint::DeleteById(handle, DebuggerBreakpointOwner::Agent, &cleared) && cleared;
}

uint64_t DEBUG_AgentConsumeLastBreakpoint(void)
{
	return CBreakpoint::ConsumeLastTriggered();
}

void DEBUG_AgentClearLastBreakpoint(void)
{
	(void)CBreakpoint::ConsumeLastTriggered();
}


#endif // C_DEBUG && C_DOSBOX_AGENT

bool DEBUG_ExitLoop(void)
{
#if C_HEAVY_DEBUG
	DrawVariables();
#endif

	if (exitLoop) {
		exitLoop = false;
		return true;
	}
	return false;
}

/********************/
/*   Draw windows   */
/********************/

static void DrawData(void) {
	if (dbg.win_main == NULL || dbg.win_data == NULL)
		return;

	uint8_t ch;
	uint32_t add = dataOfs;
	uint64_t address;
	int w,h,y;

	/* Data win */	
	getmaxyx(dbg.win_data,h,w);

	if ((paging.enabled || cpu.pmode) && dbg.data_view != DBGBlock::DATV_PHYSICAL) h--;

	for (y=0;y<h;y++) {
		// Address
		if (dbg.data_view == DBGBlock::DATV_SEGMENTED) {
			wattrset (dbg.win_data,0);
			mvwprintw (dbg.win_data,y,0,"%04X:%08X ",dataSeg,add);
		}
		else {
			wattrset (dbg.win_data,0);
			mvwprintw (dbg.win_data,y,0,"     %08X ",add);
		}

		if (dbg.data_view == DBGBlock::DATV_PHYSICAL) {
			for (int x=0; x<16; x++) {
				ch = physdev_readb(add);
				wattrset (dbg.win_data,0);
				mvwprintw (dbg.win_data,y,14+3*x,"%02X",ch);
				if(showPrintable) {
					if (ch<32 || !isprint(ch)) ch='.';
					mvwaddch(dbg.win_data, y, 63 + x, ch);
				} else {
#ifdef __PDCURSES__
					mvwaddrawch(dbg.win_data, y, 63 + x, ch);
#else
					if(ch < 32) ch = '.';
					mvwaddch(dbg.win_data, y, 63 + x, ch);
#endif
				}

				add++;
			}
		}
		else {
			for (int x=0; x<16; x++) {
				if (dbg.data_view == DBGBlock::DATV_SEGMENTED)
					address = GetAddress(dataSeg,add);
				else
					address = add;

				if (address != mem_no_address) {
					if (!mem_readb_checked((PhysPt)address,&ch)) {
						wattrset (dbg.win_data,0);
						mvwprintw (dbg.win_data,y,14+3*x,"%02X",ch);
						if(showPrintable) {
							if (ch<32 || !isprint(ch)) ch='.';
							mvwprintw (dbg.win_data,y,63+x,"%c",ch);
						} else mvwaddch(dbg.win_data,y,63+x,ch);
					}
					else {
						wattrset (dbg.win_data, COLOR_PAIR(PAIR_BYELLOW_BLACK));
						mvwprintw (dbg.win_data,y,14+3*x,"pf");
						mvwprintw (dbg.win_data,y,63+x,".");
					}
				}
				else {
					wattrset (dbg.win_data, COLOR_PAIR(PAIR_BYELLOW_BLACK));
					mvwprintw (dbg.win_data,y,14+3*x,"na");
					mvwprintw (dbg.win_data,y,63+x,".");
				}

				add++;
			}
		}
	}

	if ((paging.enabled || cpu.pmode) && dbg.data_view != DBGBlock::DATV_PHYSICAL) {
		/* one line was set aside for this information */
		wattrset (dbg.win_data,0);
		if (dbg.data_view == DBGBlock::DATV_SEGMENTED) {
			address = GetAddress(dataSeg,dataOfs);
			if (address != mem_no_address)
				mvwprintw (dbg.win_data,y,0," LIN=%08X ",(unsigned int)address);
			else
				mvwprintw (dbg.win_data,y,0," LIN=XXXXXXXX ");
		}
		else {
			address = dataOfs;
			mvwprintw (dbg.win_data,y,0,"              ");
		}

		if (!mem_readb_checked((PhysPt)address,&ch)) {
			Bitu naddr = PAGING_GetPhysicalAddress((PhysPt)address);
			mvwprintw (dbg.win_data,y,14,"PHY=%08X ",(unsigned int)naddr);
		}
		else {
			mvwprintw (dbg.win_data,y,14,"PHY=XXXXXXXX ");
		}

		wclrtoeol(dbg.win_data);

		y++;
	}

	wrefresh(dbg.win_data);
}

void DrawRegistersUpdateOld(void) {
	/* Main Registers */
	oldregs.eax=reg_eax;
	oldregs.ebx=reg_ebx;
	oldregs.ecx=reg_ecx;
	oldregs.edx=reg_edx;

	oldregs.esi=reg_esi;
	oldregs.edi=reg_edi;
	oldregs.ebp=reg_ebp;
	oldregs.esp=reg_esp;
	oldregs.eip=reg_eip;

	oldsegs[ds].val=SegValue(ds);
	oldsegs[es].val=SegValue(es);
	oldsegs[fs].val=SegValue(fs);
	oldsegs[gs].val=SegValue(gs);
	oldsegs[ss].val=SegValue(ss);
	oldsegs[cs].val=SegValue(cs);
	
	oldfpu = fpu; /* Slow? */

	/*Individual flags*/
	oldflags = reg_flags;

	oldcpucpl=cpu.cpl;
}

extern bool do_pse;

bool CPU_IsHLTed(void);

static void DrawRegisters(void) {
	if (dbg.win_main == NULL || dbg.win_reg == NULL)
		return;

	/* Main Registers */
	SetColor(reg_eax!=oldregs.eax);mvwprintw (dbg.win_reg,0,4,"%08X",reg_eax);
	SetColor(reg_ebx!=oldregs.ebx);mvwprintw (dbg.win_reg,1,4,"%08X",reg_ebx);
	SetColor(reg_ecx!=oldregs.ecx);mvwprintw (dbg.win_reg,2,4,"%08X",reg_ecx);
	SetColor(reg_edx!=oldregs.edx);mvwprintw (dbg.win_reg,3,4,"%08X",reg_edx);

	SetColor(reg_esi!=oldregs.esi);mvwprintw (dbg.win_reg,0,18,"%08X",reg_esi);
	SetColor(reg_edi!=oldregs.edi);mvwprintw (dbg.win_reg,1,18,"%08X",reg_edi);
	SetColor(reg_ebp!=oldregs.ebp);mvwprintw (dbg.win_reg,2,18,"%08X",reg_ebp);
	SetColor(reg_esp!=oldregs.esp);mvwprintw (dbg.win_reg,3,18,"%08X",reg_esp);
	SetColor(reg_eip!=oldregs.eip);mvwprintw (dbg.win_reg,1,42,"%08X",reg_eip);

	SetColor(SegValue(ds)!=oldsegs[ds].val);mvwprintw (dbg.win_reg,0,31,"%04X",SegValue(ds));
	SetColor(SegValue(es)!=oldsegs[es].val);mvwprintw (dbg.win_reg,0,41,"%04X",SegValue(es));
	SetColor(SegValue(fs)!=oldsegs[fs].val);mvwprintw (dbg.win_reg,0,51,"%04X",SegValue(fs));
	SetColor(SegValue(gs)!=oldsegs[gs].val);mvwprintw (dbg.win_reg,0,61,"%04X",SegValue(gs));
	SetColor(SegValue(ss)!=oldsegs[ss].val);mvwprintw (dbg.win_reg,0,71,"%04X",SegValue(ss));
	SetColor(SegValue(cs)!=oldsegs[cs].val);mvwprintw (dbg.win_reg,1,31,"%04X",SegValue(cs));
	
	char x87buf[12] = {};
	SetColor(F80TestUpdate(FPU_StackIndex(0)));mvwprintw (dbg.win_reg,4,4,"%s", F80ToString(FPU_StackIndex(0), x87buf));
	SetColor(F80TestUpdate(FPU_StackIndex(4)));mvwprintw (dbg.win_reg,5,4,"%s", F80ToString(FPU_StackIndex(4), x87buf));
	
	SetColor(F80TestUpdate(FPU_StackIndex(1)));mvwprintw (dbg.win_reg,4,18,"%s", F80ToString(FPU_StackIndex(1), x87buf));
	SetColor(F80TestUpdate(FPU_StackIndex(5)));mvwprintw (dbg.win_reg,5,18,"%s", F80ToString(FPU_StackIndex(5), x87buf));
	
	SetColor(F80TestUpdate(FPU_StackIndex(2)));mvwprintw (dbg.win_reg,4,32,"%s", F80ToString(FPU_StackIndex(2), x87buf));
	SetColor(F80TestUpdate(FPU_StackIndex(6)));mvwprintw (dbg.win_reg,5,32,"%s", F80ToString(FPU_StackIndex(6), x87buf));
	
	SetColor(F80TestUpdate(FPU_StackIndex(3)));mvwprintw (dbg.win_reg,4,46,"%s", F80ToString(FPU_StackIndex(3), x87buf));
	SetColor(F80TestUpdate(FPU_StackIndex(7)));mvwprintw (dbg.win_reg,5,46,"%s", F80ToString(FPU_StackIndex(7), x87buf));

	/*Individual flags*/
	Bitu changed_flags = reg_flags ^ oldflags;

	SetColor(changed_flags&FLAG_CF);
	mvwprintw (dbg.win_reg,1,53,"%01X",GETFLAG(CF) ? 1:0);
	SetColor(changed_flags&FLAG_ZF);
	mvwprintw (dbg.win_reg,1,56,"%01X",GETFLAG(ZF) ? 1:0);
	SetColor(changed_flags&FLAG_SF);
	mvwprintw (dbg.win_reg,1,59,"%01X",GETFLAG(SF) ? 1:0);
	SetColor(changed_flags&FLAG_OF);
	mvwprintw (dbg.win_reg,1,62,"%01X",GETFLAG(OF) ? 1:0);
	SetColor(changed_flags&FLAG_AF);
	mvwprintw (dbg.win_reg,1,65,"%01X",GETFLAG(AF) ? 1:0);
	SetColor(changed_flags&FLAG_PF);
	mvwprintw (dbg.win_reg,1,68,"%01X",GETFLAG(PF) ? 1:0);


	SetColor(changed_flags&FLAG_DF);
	mvwprintw (dbg.win_reg,1,71,"%01X",GETFLAG(DF) ? 1:0);
	SetColor(changed_flags&FLAG_IF);
	mvwprintw (dbg.win_reg,1,74,"%01X",GETFLAG(IF) ? 1:0);
	SetColor(changed_flags&FLAG_TF);
	mvwprintw (dbg.win_reg,1,77,"%01X",GETFLAG(TF) ? 1:0);

	SetColor(changed_flags&FLAG_IOPL);
	mvwprintw (dbg.win_reg,2,72,"%01X",(unsigned int)(GETFLAG(IOPL)>>12));


	SetColor(cpu.cpl ^ oldcpucpl);
	mvwprintw (dbg.win_reg,2,78,"%01X",(unsigned int)cpu.cpl);

	if (cpu.pmode) {
		if (reg_flags & FLAG_VM) mvwprintw(dbg.win_reg,0,76,"VM86");
		else if (cpu.code.big) mvwprintw(dbg.win_reg,0,76,"Pr32");
		else mvwprintw(dbg.win_reg,0,76,"Pr16");
		if (paging.enabled) {
			if (do_pse)
				mvwprintw(dbg.win_reg,2,60,"PAGEPSE");
			else
				mvwprintw(dbg.win_reg,2,60,"PAGE");
		}
		else {
			mvwprintw(dbg.win_reg,2,60,"NOPG");
		}
	} else {
		mvwprintw(dbg.win_reg,0,76,"Real");
		mvwprintw(dbg.win_reg,2,62,"NOPG");
	}

	// Selector info, if available
	if ((cpu.pmode) && curSelectorName[0]) {
		char out1[200], out2[200];
		GetDescriptorInfo(curSelectorName,out1,out2);
		mvwprintw(dbg.win_reg, 2, 28, "%s", out1);
		mvwprintw(dbg.win_reg, 3, 28, "%s", out2);
	}

	wattrset(dbg.win_reg,0);

	mvwprintw(dbg.win_reg,3,60,"cc=%-8u ",(unsigned int)cycle_count);
	if (CPU_IsHLTed()) mvwprintw(dbg.win_reg,3,73,"HLT ");
	else mvwprintw(dbg.win_reg,3,73,"RUN ");

	mvwprintw(dbg.win_reg,4,60,"pfi=%-6.9f ",(double)PIC_FullIndex());

	wrefresh(dbg.win_reg);
}

bool DEBUG_IsPagingOutput(void);

static void DrawInput(void) {
	if (dbg.win_inp == NULL)
		return;

    if (!debugging) {
        if (has_colors())
        {
        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        }

        mvwprintw(dbg.win_inp,0,0,"%s","(Running)");
        wclrtoeol(dbg.win_inp);
    } else if (debug_running) {
        if (has_colors())
        {
        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        }

        mvwprintw(dbg.win_inp,0,0,"%s","(Running/watching)");
        wclrtoeol(dbg.win_inp);
    } else if (DEBUG_IsPagingOutput()) {
        if (has_colors())
        {
        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_GREEN_BLACK));
        }

        mvwprintw(dbg.win_inp,0,0,"%s","^ Paged content: Hit ENTER to continue, Q to exit paging");
        wclrtoeol(dbg.win_inp);
    } else {
        //TODO long lines
        char* dispPtr = codeViewData.inputStr;
        char* curPtr = &codeViewData.inputStr[codeViewData.inputPos];

        wbkgdset(dbg.win_inp,COLOR_PAIR(PAIR_BLACK_GREY));
        wattrset(dbg.win_inp,COLOR_PAIR(PAIR_BLACK_GREY));
        mvwprintw(dbg.win_inp,0,0,"%c-> %s%c",
                (codeViewData.ovrMode?'O':'I'),dispPtr,(*curPtr?' ':'_'));
        wclrtoeol(dbg.win_inp); // not correct in pdcurses if full line
        if (*curPtr) {
            mvwchgat(dbg.win_inp,0,(int)(curPtr-dispPtr+4),1,A_REVERSE,(PAIR_BLACK_GREY),NULL);
        } 
    }

    wattrset(dbg.win_inp,0);
    wrefresh(dbg.win_inp);
}

static void DrawCode(void) {
	if (dbg.win_main == NULL || dbg.win_code == NULL)
		return;

	uint32_t disEIP = codeViewData.useEIP;
	char dline[200];Bitu size;
	static char line20[21] = "                    ";
    int w,h;

    getmaxyx(dbg.win_code,h,w);
	for (int i=0;i<h;i++) {
        uint64_t start = GetAddress(codeViewData.useCS,disEIP);

		bool saveSel = false;
		if (has_colors()) {
			if ((codeViewData.useCS==SegValue(cs)) && (disEIP == reg_eip)) {
				if (codeViewData.cursorPos==-1) {
					codeViewData.cursorPos = i; // Set Cursor
				}
				if (i == codeViewData.cursorPos) {
					codeViewData.cursorSeg = SegValue(cs);
					codeViewData.cursorOfs = disEIP;
				}
				saveSel = (i == codeViewData.cursorPos);

                if (i == codeViewData.cursorPos) {
                    wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREEN));
                    wattrset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREEN));
                }
                else {
                    wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));
                    wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));
                }
            } else if (i == codeViewData.cursorPos) {
                wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREY));
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREY));
				codeViewData.cursorSeg = codeViewData.useCS;
				codeViewData.cursorOfs = disEIP;
				saveSel = true;
			} else if (CBreakpoint::IsBreakpoint(codeViewData.useCS, disEIP)) {
                wbkgdset(dbg.win_code,COLOR_PAIR(PAIR_GREY_RED));
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREY_RED));
			} else {
                wbkgdset(dbg.win_code,0);
                wattrset(dbg.win_code,0);
			}
		}


		bool toolarge = false;
        bool no_bytes = false;
		Bitu drawsize;

        if (start != mem_no_address) {
            Descriptor desc;

            if (cpu.gdt.GetDescriptor(codeViewData.useCS, desc))
                drawsize=size=DasmI386(dline, (PhysPt)start, disEIP, desc.saved.seg.big);
            else
                drawsize=size=DasmI386(dline, (PhysPt)start, disEIP, cpu.code.big);
        }
        else {
            drawsize=size=1;
            dline[0]=0;
        }
		AnnotateDirectBranch(dline, sizeof(dline));
		mvwprintw(dbg.win_code,i,0,"%04X:%08X ",codeViewData.useCS,disEIP);

		if (drawsize>10) { toolarge = true; drawsize = 9; }
 
        if (start != mem_no_address) {
            for (Bitu c=0;c<drawsize;c++) {
                uint8_t value;
                if (!mem_readb_checked((PhysPt)(start+c),&value)) {
                    wattrset (dbg.win_code,0);
                    wprintw(dbg.win_code,"%02X",value);
                }
                else {
                    no_bytes = true;
                    wattrset (dbg.win_code, COLOR_PAIR(PAIR_BYELLOW_BLACK));
                    wprintw(dbg.win_code,"pf");
                }
            }
        }
        else {
            wattrset (dbg.win_code, COLOR_PAIR(PAIR_BYELLOW_BLACK));
            wprintw(dbg.win_code,"na");
        }

        wattrset (dbg.win_code,0);
        if (toolarge) { waddstr(dbg.win_code,".."); drawsize++; }
		// Spacepad up to 20 characters
		if(drawsize && (drawsize < 11)) {
			line20[20 - drawsize*2] = 0;
			waddstr(dbg.win_code,line20);
			line20[20 - drawsize*2] = ' ';
		} else waddstr(dbg.win_code,line20);

		char empty_res[] = { 0 };
		char* res = empty_res;
        wattrset (dbg.win_code,0);
        if (showExtend) res = AnalyzeInstruction(dline, saveSel);
		// Spacepad it up to 28 characters
        if (no_bytes) dline[0] = 0;
		size_t dline_len = strlen(dline);
        if (dline_len < 28) {
            memset(dline + dline_len, ' ', 28 - dline_len);
            dline[28] = 0;
        }
		waddstr(dbg.win_code,dline);
		// Spacepad it up to 20 characters
		size_t res_len = strlen(res);
		if(res_len && (res_len < 21)) {
			waddstr(dbg.win_code,res);
			line20[20-res_len] = 0;
			waddstr(dbg.win_code,line20);
			line20[20-res_len] = ' ';
		} else 	waddstr(dbg.win_code,line20);

        wclrtoeol(dbg.win_code);

        disEIP+=(uint32_t)size;

		if (i==0) codeViewData.firstInstSize = (uint16_t)size;
		if (i==4) codeViewData.useEIPmid	 = disEIP;
	}

	codeViewData.useEIPlast = disEIP;

	wrefresh(dbg.win_code);
}

void getcodetext(std::string &text) {
	text = "";
	uint32_t disEIP = reg_eip;
	char dline[200],tmp[200];Bitu size;
	static char line15[16] = "               ";

	for (int i=0;i<0x400;i++) {
        uint64_t start = GetAddress(SegValue(cs),disEIP);
        bool no_bytes = false;
		Bitu drawsize;
        if (start != mem_no_address)
            drawsize=size=DasmI386(dline, (PhysPt)start, disEIP, cpu.code.big);
        else {
            drawsize=size=1;
            dline[0]=0;
        }
		sprintf(tmp,"%04X:%08X ",SegValue(cs),disEIP);
        text += tmp;
		if (drawsize>10) drawsize = 9;
        if (start != mem_no_address) {
            for (Bitu c=0;c<drawsize;c++) {
                uint8_t value;
                if (!mem_readb_checked((PhysPt)(start+c),&value)) {
                    sprintf(tmp,"%02X",value);
                    text += tmp;
                } else {
                    no_bytes = true;
                    text += "pf";
                }
            }
        }
        else
            text += "na";

		// Spacepad up to 15 characters
		if(drawsize && (drawsize < 11)) {
			line15[15 - drawsize*2] = 0;
            text += line15;
			line15[15 - drawsize*2] = ' ';
		} else
            text += line15;

		char empty_res[] = { 0 };
		char* res = empty_res;
        if (showExtend) res = AnalyzeInstruction(dline, false);
		// Spacepad it up to 23 characters
        if (no_bytes) dline[0] = 0;
		size_t dline_len = strlen(dline);
        if (dline_len < 23) {
            memset(dline + dline_len, ' ', 23 - dline_len);
            dline[23] = 0;
        }
        text += dline;
		// Spacepad it up to 15 characters
		size_t res_len = strlen(res);
		if(res_len && (res_len < 16)) {
            text += res;
			line15[15-res_len] = 0;
            text += line15;
			line15[15-res_len] = ' ';
		} else
            text += line15;
        disEIP+=(uint32_t)size;
	}
}

static void SetCodeWinStart()
{
	if ((SegValue(cs)==codeViewData.useCS) && (reg_eip>=codeViewData.useEIP) && (reg_eip<=codeViewData.useEIPlast)) {
		// in valid window - scroll ?
		if (reg_eip>=codeViewData.useEIPmid) codeViewData.useEIP += codeViewData.firstInstSize;

	} else {
		// totally out of range.
		codeViewData.useCS	= SegValue(cs);
		codeViewData.useEIP	= reg_eip;
	}
	codeViewData.cursorPos = -1;	// Recalc Cursor position
}

void DEBUG_CheckCSIP() {
    SetCodeWinStart();
}

/********************/
/*    User input    */
/********************/

void SkipSpace(char*& hex) {
    while (*hex == ' ') hex++;
}

extern uint32_t cpu_sep_eip;

uint32_t GetHexValue(char* const str, char* &hex,bool *parsed,int exprge)
{
    uint32_t regval = 0;
    hex = str;
    while (*hex == ' ') hex++;

    std::string something;
    bool hexnumber = true;
    bool quoted = false;
    char *start;

    if (parsed) *parsed = 1;

    // parenthesis handling (expr)
    if (*hex == '(') {
        bool myparsed = false;

        hex++;
        regval = GetHexValue(hex,hex,&myparsed);
        if (!myparsed) return 0;

        while (*hex == ' ') hex++;
        if (*hex != ')') { // syntax error
                if (parsed) *parsed = 0;
                return 0;
        }
        hex++; /* then skip the ) */
    }
    else {
        // In this code base, scan the string for alphanumeric characters, store the string,
        // THEN process for identifiers. This is to make sure "DFE" is not confused with
        // the DF flag, for example.
        if (*hex == '\"') {
            quoted = true;
            start = ++hex; /* start = first char after quote */
            while (*hex != 0 && *hex != '\"') {
                if (!isxdigit(*hex)) hexnumber = false;
                hex++;
            }
            something = std::string(start,(size_t)(hex-start));
            if (*hex == '\"') hex++;
        }
        else {
            start = hex;
            while (isalpha(*hex) || isdigit(*hex) || *hex == '_') {
                if (!isxdigit(*hex)) hexnumber = false;
                hex++;
            }
            something = std::string(start,(size_t)(hex-start));
        }

        // "" is not valid
        if (something.empty()) { if (parsed) *parsed = 0; return 0; }

        // The user can enclose a value in double quotations to enter hex values that
        // would collide with a flag name (AC, AF, CF, and DF).
        if (!quoted) {
                 if (something == "EFLAGS") { regval = (uint32_t)reg_flags; }
            else if (something == "FLAGS") { regval = (uint32_t)reg_flags; }
            else if (something == "IOPL") { regval = (reg_flags & FLAG_IOPL) >> 12u; }
            else if (something == "CR0") { regval = (uint32_t)cpu.cr0; }
            else if (something == "CR2") { regval = (uint32_t)paging.cr2; }
            else if (something == "CR3") { regval = (uint32_t)paging.cr3; }
            else if (something == "CR4") { regval = (uint32_t)cpu.cr4; }
            else if (something == "SYSENTER") { regval = (uint32_t)cpu_sep_eip; }
            else if (something == "EAX") { regval = reg_eax; }
            else if (something == "EBX") { regval = reg_ebx; }
            else if (something == "ECX") { regval = reg_ecx; }
            else if (something == "EDX") { regval = reg_edx; }
            else if (something == "ESI") { regval = reg_esi; }
            else if (something == "EDI") { regval = reg_edi; }
            else if (something == "EBP") { regval = reg_ebp; }
            else if (something == "ESP") { regval = reg_esp; }
            else if (something == "EIP") { regval = reg_eip; }
            else if (something == "AX") { regval = reg_ax; }
            else if (something == "BX") { regval = reg_bx; }
            else if (something == "CX") { regval = reg_cx; }
            else if (something == "DX") { regval = reg_dx; }
            else if (something == "SI") { regval = reg_si; }
            else if (something == "DI") { regval = reg_di; }
            else if (something == "BP") { regval = reg_bp; }
            else if (something == "SP") { regval = reg_sp; }
            else if (something == "IP") { regval = reg_ip; }
            else if (something == "AL") { regval = reg_al; }
            else if (something == "BL") { regval = reg_bl; }
            else if (something == "CL") { regval = reg_cl; }
            else if (something == "DL") { regval = reg_dl; }
            else if (something == "AH") { regval = reg_ah; }
            else if (something == "BH") { regval = reg_bh; }
            else if (something == "CH") { regval = reg_ch; }
            else if (something == "DH") { regval = reg_dh; }
            else if (something == "CS") { regval = SegValue(cs); }
            else if (something == "DS") { regval = SegValue(ds); }
            else if (something == "ES") { regval = SegValue(es); }
            else if (something == "FS") { regval = SegValue(fs); }
            else if (something == "GS") { regval = SegValue(gs); }
            else if (something == "SS") { regval = SegValue(ss); }
            else if (something == "AC") { regval = GETFLAG(AC); }
            else if (something == "AF") { regval = GETFLAG(AF); }
            else if (something == "CF") { regval = GETFLAG(CF); }
            else if (something == "DF") { regval = GETFLAG(DF); }
            else if (something == "ID") { regval = GETFLAG(ID); }
            else if (something == "IF") { regval = GETFLAG(IF); }
            else if (something == "NT") { regval = GETFLAG(NT); }
            else if (something == "OF") { regval = GETFLAG(OF); }
            else if (something == "PF") { regval = GETFLAG(PF); }
            else if (something == "SF") { regval = GETFLAG(SF); }
            else if (something == "TF") { regval = GETFLAG(TF); }
            else if (something == "VM") { regval = GETFLAG(VM); }
            else if (something == "ZF") { regval = GETFLAG(ZF); }

            else if (something == "DTASEG") { regval = (!dos_kernel_disabled) ? (dos.dta() >> 16u)    : 0; }
            else if (something == "DTAOFF") { regval = (!dos_kernel_disabled) ? (dos.dta() & 0xFFFFu) : 0; }
            else if (something == "PSPSEG") { regval = (!dos_kernel_disabled) ?  dos.psp()            : 0; }
            else if (CDebugVar* variable = CDebugVar::FindVar(something)) { regval = variable->GetAdr(); }
            else if (hexnumber) { regval = (uint32_t)strtoul(something.c_str(),NULL,16/*hexadecimal*/); }
            else { if (parsed) *parsed = 0; return 0; }
        }
        /* quoted */
        else if (hexnumber) { regval = (uint32_t)strtoul(something.c_str(),NULL,16/*hexadecimal*/); }
        else { if (parsed) *parsed = 0; return 0; }
    }

    /* support simple add/subtract expressions */
    while (*hex != 0) {
        char* beforespace = hex;
        while (*hex == ' ') hex++;

        if (*hex == '+') {
            if (exprge >= EXPR_ADDSUB) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            regval += GetHexValue(hex, hex, parsed, EXPR_ADDSUB);
        }
        else if (*hex == '-') {
            if (exprge >= EXPR_ADDSUB) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            regval -= GetHexValue(hex, hex, parsed, EXPR_ADDSUB);
        }
	else if (*hex == '*') {
            if (exprge >= EXPR_MULDIV) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            regval *= GetHexValue(hex, hex, parsed, EXPR_MULDIV);
        }
        else if (*hex == '/') {
            if (exprge >= EXPR_MULDIV) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            uint32_t r = GetHexValue(hex, hex, parsed, EXPR_MULDIV); /* whoah now, check for zero! */
            if (r != 0) regval /= r;
            else regval = 0;
        }
        else if (*hex == '%') {
            if (exprge >= EXPR_MULDIV) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            uint32_t r = GetHexValue(hex, hex, parsed, EXPR_MULDIV); /* whoah now, check for zero! */
            if (r != 0) regval %= r;
            else regval = 0;
        }
        else if (*hex == '&') {
            if (exprge >= EXPR_AND) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            regval &= GetHexValue(hex, hex, parsed, EXPR_AND);
        }
        else if (*hex == '^') {
            if (exprge >= EXPR_XOR) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            regval ^= GetHexValue(hex, hex, parsed, EXPR_XOR);
        }
        else if (*hex == '|') {
            if (exprge >= EXPR_OR) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex++;
            regval |= GetHexValue(hex, hex, parsed, EXPR_OR);
        }
        else if (hex[0] == '<' && hex[1] == '<') {
            if (exprge >= EXPR_BITSHIFT) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex += 2;
            uint32_t r = GetHexValue(hex, hex, parsed, EXPR_BITSHIFT);
            if (r < 32) regval <<= r;
            else regval = 0;
        }
        else if (hex[0] == '>' && hex[1] == '>') {
            if (exprge >= EXPR_BITSHIFT) break; /* if order of operations says we're handling something higher precedence, stop now */
            hex += 2;
            uint32_t r = GetHexValue(hex, hex, parsed, EXPR_BITSHIFT);
            if (r < 32) regval >>= r;
            else regval = 0;
        }
        else {
            hex = beforespace; // Rewind to avoid consuming trailing spaces
            break; // No valid char
        }
    }

    return regval;
}

bool ChangeRegister(char* const str)
{
	char* hex = str;

    while (*hex) {
    	while (*hex==' ') hex++;

    	if (strstr(hex,"EFLAGS")==hex) { hex+=6; CPU_SetFlags(GetHexValue(hex,hex),FMASK_ALL); } else
    	if (strstr(hex,"FLAGS")==hex) { hex+=5; CPU_SetFlags(GetHexValue(hex,hex),FMASK_ALL & 0xFFFFu); } else

        //             "IOPL"

    	if (strncmp(hex,"EAX",3) == 0) { hex+=3; reg_eax = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EBX",3) == 0) { hex+=3; reg_ebx = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"ECX",3) == 0) { hex+=3; reg_ecx = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EDX",3) == 0) { hex+=3; reg_edx = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"ESI",3) == 0) { hex+=3; reg_esi = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EDI",3) == 0) { hex+=3; reg_edi = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EBP",3) == 0) { hex+=3; reg_ebp = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"ESP",3) == 0) { hex+=3; reg_esp = GetHexValue(hex,hex); } else
    	if (strncmp(hex,"EIP",3) == 0) { hex+=3; reg_eip = GetHexValue(hex,hex); } else

    	if (strncmp(hex,"AX",2) == 0) { hex+=2; reg_ax = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BX",2) == 0) { hex+=2; reg_bx = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"CX",2) == 0) { hex+=2; reg_cx = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DX",2) == 0) { hex+=2; reg_dx = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"SI",2) == 0) { hex+=2; reg_si = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DI",2) == 0) { hex+=2; reg_di = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BP",2) == 0) { hex+=2; reg_bp = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"SP",2) == 0) { hex+=2; reg_sp = (uint16_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"IP",2) == 0) { hex+=2; reg_ip = (uint16_t)GetHexValue(hex,hex); } else

	    if (strncmp(hex,"AL",2) == 0) { hex+=2; reg_al = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BL",2) == 0) { hex+=2; reg_bl = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"CL",2) == 0) { hex+=2; reg_cl = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DL",2) == 0) { hex+=2; reg_dl = (uint8_t)GetHexValue(hex,hex); } else

    	if (strncmp(hex,"AH",2) == 0) { hex+=2; reg_ah = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"BH",2) == 0) { hex+=2; reg_bh = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"CH",2) == 0) { hex+=2; reg_ch = (uint8_t)GetHexValue(hex,hex); } else
    	if (strncmp(hex,"DH",2) == 0) { hex+=2; reg_dh = (uint8_t)GetHexValue(hex,hex); } else

    	if (strncmp(hex,"CS",2) == 0) { hex+=2; SegSet16(cs,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"DS",2) == 0) { hex+=2; SegSet16(ds,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"ES",2) == 0) { hex+=2; SegSet16(es,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"FS",2) == 0) { hex+=2; SegSet16(fs,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"GS",2) == 0) { hex+=2; SegSet16(gs,(uint16_t)GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"SS",2) == 0) { hex+=2; SegSet16(ss,(uint16_t)GetHexValue(hex,hex)); } else

        if (strncmp(hex,"AC",2) == 0) { hex+=2; SETFLAGBIT(AC,GetHexValue(hex,hex)); } else
        if (strncmp(hex,"AF",2) == 0) { hex+=2; SETFLAGBIT(AF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"CF",2) == 0) { hex+=2; SETFLAGBIT(CF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"DF",2) == 0) { hex+=2; SETFLAGBIT(DF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"ID",2) == 0) { hex+=2; SETFLAGBIT(ID,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"IF",2) == 0) { hex+=2; SETFLAGBIT(IF,GetHexValue(hex,hex)); } else
        //             "NT"
    	if (strncmp(hex,"OF",2) == 0) { hex+=2; SETFLAGBIT(OF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"PF",2) == 0) { hex+=2; SETFLAGBIT(PF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"SF",2) == 0) { hex+=2; SETFLAGBIT(SF,GetHexValue(hex,hex)); } else
    	if (strncmp(hex,"TF",2) == 0) { hex+=2; SETFLAGBIT(TF,GetHexValue(hex,hex)); } else
        //             "VM"
	    if (strncmp(hex,"ZF",2) == 0) { hex+=2; SETFLAGBIT(ZF,GetHexValue(hex,hex)); } else
    	{ return false; }
    }

	return true;
}

void DEBUG_PrintMMX(int which,char format) {
    char tmp[256],*w=tmp;

    if (which < 0 || which > 7) return;

    MMX_reg &mmx = *reg_mmx[which];

    w += sprintf(w,"mm%d(%c): H->L ",which,format); /* let the user know we're printing from little endian order highest to lowest */
    if (format == 'B') {
        w += sprintf(w,"%02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x",
            mmx.ub.b7,mmx.ub.b6,mmx.ub.b5,mmx.ub.b4,
            mmx.ub.b3,mmx.ub.b2,mmx.ub.b1,mmx.ub.b0);
    }
    else if (format == 'W') {
        w += sprintf(w,"%04x|%04x|%04x|%04x",
            mmx.uw.w3,mmx.uw.w2,mmx.uw.w1,mmx.uw.w0);
    }
    else if (format == 'D') {
        w += sprintf(w,"%08lx|%08lx",
            (unsigned long)mmx.ud.d1,(unsigned long)mmx.ud.d0);
    }
    else if (format == 'S') {
        w += sprintf(w,"%.20f|%.20f",
            mmx.f32.f1.v,mmx.f32.f0.v);
    }
    else { /* 'Q' */
        w += sprintf(w,"%08lx%08lx",
            (unsigned long)mmx.ud.d1,(unsigned long)mmx.ud.d0); /* little endian combine to print 64-bit hex */
    }

    assert(w < (tmp+sizeof(tmp)));
    DEBUG_ShowMsg("%s", tmp);
}

void DEBUG_PrintSSE(int which,char format) {
    char tmp[1024],*w=tmp;

    if (which < 0 || which > 7) return;

    XMM_Reg &xmm = fpu.xmmreg[which];

    w += sprintf(w,"xmm%d(%c): H->L ",which,format); /* let the user know we're printing from little endian order highest to lowest */
    if (format == 'B') {
        for (int i=15;i >= 0;i--) {
            w += sprintf(w,"%02x",xmm.u8[(unsigned int)i]);
            if (i != 0) *w++ = '|';
        }
    }
    else if (format == 'W') {
        for (int i=7;i >= 0;i--) {
            w += sprintf(w,"%04x",xmm.u16[(unsigned int)i]);
            if (i != 0) *w++ = '|';
        }
    }
    else if (format == 'D') {
        for (int i=3;i >= 0;i--) {
            w += sprintf(w,"%08lx",(unsigned long)xmm.u32[(unsigned int)i]);
            if (i != 0) *w++ = '|';
        }
    }
    else if (format == 'Q') {
        for (int i=3;i >= 0;i--) { /* rather than fight with printf() differences between MSVC++ and glibc just print 32-bit values anyway */
            w += sprintf(w,"%08lx",(unsigned long)xmm.u32[(unsigned int)i]);
            if ((i&1u) == 0 && i != 0) *w++ = '|';
        }
    }
    else if (format == 'S') {
        for (int i=3;i >= 0;i--) {
            w += sprintf(w,"%.20f",xmm.f32[(unsigned int)i].v);
            if (i != 0) *w++ = '|';
        }
    }
    else if (format == 'F') {
        for (int i=1;i >= 0;i--) {
            w += sprintf(w,"%.30f",xmm.f64[(unsigned int)i].v);
            if (i != 0) *w++ = '|';
        }
    }
    else { /* 'X' */
        for (int i=3;i >= 0;i--) { /* rather than fight with printf() differences between MSVC++ and glibc just print 32-bit values anyway */
            w += sprintf(w,"%08lx",(unsigned long)xmm.u32[(unsigned int)i]);
        }
    }

    assert(w < (tmp+sizeof(tmp)));
    DEBUG_ShowMsg("%s", tmp);
}

void DEBUG_GUI_Rebuild(void);
void DBGUI_NextWindowIfActiveHidden(void);

void DEBUG_BeginPagedContent(void);
void DEBUG_EndPagedContent(void);

std::string pc98_egc_shift_debug_status(void);

static void LogFPUInfo(void);

bool lookslikefloat(char* str) {
    if (*str == 0) return false;

    if (*str == '-') str++;
    while (!(*str == 0 || *str == ' ' || *str == ',' || *str == '.')) {
        if (!isdigit(*str)) return false;
        str++;
    }

    if (*str != '.') return false;
    str++;

    while (!(*str == 0 || *str == ' ' || *str == ',')) {
        if (!isdigit(*str)) return false;
        str++;
    }

    return true;
}

void AddBPINT3(void) {
	CBreakpoint::AddIntBreakpoint(3,BPINT_ALL,BPINT_ALL,false);
	CBreakpoint::ActivateBreakpoints();
}

void VGA_DumpFontRamBIN(const char *filename);
void VGA_DumpFontRamBMP(const char *filename);
int32_t DEBUG_Run(int32_t amount,bool quickexit);

static bool ParseMemoryDumpArguments(char *&found, uint16_t &seg, uint32_t &ofs, uint32_t &num) {
	bool parsed = false;
	uint32_t segment = GetHexValue(found,found,&parsed);
	if (!parsed || segment > 0xffffu) return false;
	seg = (uint16_t)segment;

	SkipSpace(found);
	if (*found != ':') return false;
	found++;

	ofs = GetHexValue(found,found,&parsed);
	if (!parsed || *found != ' ') return false;
	SkipSpace(found);

	num = GetHexValue(found,found,&parsed);
	if (!parsed || (*found != '\0' && *found != ' ')) return false;
	SkipSpace(found);
	return true;
}

// The text parser is an adapter, not an independent mutation authority.
// Each ingress supplies its selected attachment; the parser never attaches a
// frontend implicitly. The native owner validates the held context/lease.
static NativeDebug::Attachment DEBUG_CommandSession();
static bool ParseCommandWithResult(char* str, bool *recognized) {
    // Separate operation failure from unknown-command fallback diagnostics.
    if (recognized) *recognized = true;
    if (!DEBUG_AdmitNativeEffect(DEBUG_CommandSession())) return false;
    if (!NativeControl::diagnosticEffectAllowed()) return false;
    std::string copy_str = str;
    for (auto &c : copy_str) c = toupper(c);
    copy_str += '\0'; /* paranoid */
	char* found = &(copy_str[0]); /* cannot use std::string c_str(), that is const char* */

	found = trim(found);
	string s_found(found);
	istringstream stream(s_found);
	string command;
	stream >> command;
	string::size_type next = s_found.find_first_not_of(' ',command.size());
	if(next == string::npos) next = command.size();
	(s_found.erase)(0,next);
	found = const_cast<char*>(s_found.c_str());

    if (command == "STATEWRITE" || command == "STATEREAD") {
        const char *argument = str;
        while (*argument && isspace(static_cast<unsigned char>(*argument))) ++argument;
        while (*argument && !isspace(static_cast<unsigned char>(*argument))) ++argument;
        while (*argument && isspace(static_cast<unsigned char>(*argument))) ++argument;
        if (!*argument) { DEBUG_ShowMsg("State path required\n"); return false; }
        const bool success = DEBUG_NativeStateFile(DEBUG_CommandSession(),
                DEBUG_NativeExecutionContext(), argument, command == "STATEREAD");
        DEBUG_ShowMsg(success ? "State completed\n" : "State failed\n");
        return success;
    }

    if (command == "RUNANY") {
#if defined(C_DOSBOX_AGENT)
        const char *argument=str;
        while(*argument && isspace(static_cast<unsigned char>(*argument)))++argument;
        while(*argument && !isspace(static_cast<unsigned char>(*argument)))++argument;
        dosbox_agent::JsonValue list;
        std::vector<NativeDebug::StopCondition> conditions;
        std::string error;
        if (!dosbox_agent::AGENT_ParseJson(argument,&list,&error) ||
            !dosbox_agent::ParseStopConditions(&list,conditions,error)) {
            DEBUG_ShowMsg("RUNANY rejected: %s\n",error.c_str());return false;
        }
        return DEBUG_ResumeNativeExecution(NativeDebug::Frontend::Interactive,false,false,DEBUG_CommandSession(),conditions);
#else
        DEBUG_ShowMsg("RUNANY requires the built-in Agent JSON codec\n");return false;
#endif
    }

    if (command == "LUA" || command == "LUAFILE" || command == "LUARESET") {
#if C_DEBUG_LUA
        if(command=="LUARESET")return Debugger_LuaInteractiveReset();
        // Preserve source/path case: only the command verb is case-insensitive.
        const char *argument=str;
        while(*argument && isspace(static_cast<unsigned char>(*argument)))++argument;
        while(*argument && !isspace(static_cast<unsigned char>(*argument)))++argument;
        while(*argument && isspace(static_cast<unsigned char>(*argument)))++argument;
        Debugger_LuaInteractive(argument,command=="LUAFILE",DEBUG_CommandSession());
#else
        DEBUG_ShowMsg("Lua support is not compiled in");
#endif
        return true;
    }

    if (command == "DETACH") {
        const bool detached=DEBUG_DetachNativeExecution(DEBUG_CommandSession());
        LOG_MSG("DEBUG: session detach %s",detached ? "complete" : "pending cleanup");
        return detached;
    }

    if (command == "QUIT") {
        void DoKillSwitch(void);
        DoKillSwitch();
        return true;
    }

    if (command == "MOVEWINDN") { // MOVE WINDOW DOWN (by swapping)
        int order1 = dbg.win_find_order((int)dbg.active_win);
        int order2 = dbg.win_next_by_order(order1);

        if (order1 >= 0 && order2 >= 0 && order1 < order2) {
            dbg.swap_order(order1,order2);
            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
        }

        return true;
    }

    if (command == "MOVEWINUP") { // MOVE WINDOW UP (by swapping)
        int order1 = dbg.win_find_order((int)dbg.active_win);
        int order2 = dbg.win_prev_by_order(order1);

        if (order1 >= 0 && order2 >= 0 && order1 > order2) {
            dbg.swap_order(order1,order2);
            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
        }

        return true;
    }

    if (command == "SHOWWIN") { // SHOW WINDOW <name>
        int win = dbg.name_to_win(found);
        if (win >= 0) {
            dbg.win_vis[win] = true;

            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
            return true;
        }
        else {
            LOG_MSG("No such window '%s'. Windows are: %s",found,dbg.windowlist_by_name().c_str());
            return false;
        }
    }

    if (command == "HIDEWIN") { // HIDE WINDOW <name>
        int win = dbg.name_to_win(found);
        if (win >= 0) {
            dbg.win_vis[win] = false;

            DEBUG_GUI_Rebuild();
            DBGUI_NextWindowIfActiveHidden();
            return true;
        }
        else {
            LOG_MSG("No such window '%s'. Windows are: %s",found,dbg.windowlist_by_name().c_str());
            return false;
        }
    }

	if (command == "MEMDUMP") { // Dump memory to file
		uint16_t seg;
		uint32_t ofs, num;
		if (!ParseMemoryDumpArguments(found,seg,ofs,num)) {
			DEBUG_ShowMsg("DEBUG: Invalid memory dump arguments. Usage: MEMDUMP seg:off len [filename].\n");
			return true;
		}
		SaveMemory(*found ? found : "MEMDUMP.TXT",seg,ofs,num);
		return true;
	}

	if (command == "MEMDUMPBIN") { // Dump memory to file binary
		uint16_t seg;
		uint32_t ofs, num;
		if (!ParseMemoryDumpArguments(found,seg,ofs,num)) {
			DEBUG_ShowMsg("DEBUG: Invalid memory dump arguments. Usage: MEMDUMPBIN seg:off len [filename].\n");
			return true;
		}
		SaveMemoryBin(*found ? found : "MEMDUMP.BIN",seg,ofs,num);
		return true;
	}

    if (command == "MEMFIND") { // Start a memory search instance with seg:ofs range
		uint32_t valfind;
		uint8_t valfind8;
		uint16_t valfind16;
		uint32_t valfind32;
		uint32_t listedvalues = 0;
		if (found[0] == '!'){
			if (MEMFINDInstance != NULL){
				DEBUG_ShowMsg("DEBUG: Memory search instance cancelled.");
				MEMFINDInstance->tableTruth.clear();
				MEMFINDInstance->tableValue.clear();
				MEMFINDInstance->tableTruth.shrink_to_fit();
				MEMFINDInstance->tableValue.shrink_to_fit();
				delete MEMFINDInstance;
				MEMFINDInstance = NULL;
			} else {
				DEBUG_ShowMsg("DEBUG: No active search instances to cancel.");
			}
			return true;
		} else if (found[0] == 'L'){
			if (MEMFINDInstance != NULL){
				DEBUG_BeginPagedContent();
				uint32_t j = 0;
				for (uint32_t i = MEMFINDInstance->baseLinear; i < MEMFINDInstance->baseLinear + MEMFINDInstance->range; i+=MEMFINDInstance->size){
							switch (MEMFINDInstance->size){
								case 1:
									mem_readb_checked((PhysPt)(i),&valfind8);
									valfind = valfind8;
									break;
								case 2:
									mem_readw_checked((PhysPt)(i),&valfind16);
									valfind = valfind16;
									break;
								case 4:
									mem_readd_checked((PhysPt)(i),&valfind32);
									valfind = valfind32;
									break;
								default:
									mem_readb_checked((PhysPt)(i),&valfind8);
									valfind = valfind8;
									break;
							}
							if (MEMFINDInstance->tableTruth[j] == true){
								listedvalues++;
								DEBUG_ShowMsg("DEBUG: [MEMFIND] Address: %04X:%06X (lin %08X) Current Value: %0*X\n",MEMFINDInstance->seg,(MEMFINDInstance->ofs + (i - MEMFINDInstance->baseLinear)),i,MEMFINDInstance->size*2,valfind);
							}
							j+=MEMFINDInstance->size;
						}
				DEBUG_ShowMsg("DEBUG: [MEMFIND] Matched values during current instance: %06X\n",listedvalues);
				DEBUG_EndPagedContent();
			} else {
				DEBUG_ShowMsg("DEBUG: No active search instances to list from.");
			}
			return true;
		}
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = GetHexValue(found,found); found++;
		uint32_t num = GetHexValue(found,found); found++;
		if ((MEMFINDInstance == NULL) && (num > 0)){
			uint64_t base = GetAddress(seg,ofs);
			if (base == mem_no_address){
				DEBUG_ShowMsg("DEBUG: Invalid selector/segment, cancelling.");
				return true;
			}
			uint64_t memBytes = (uint64_t)MEM_TotalPages() << 12;
			if ((base + (uint64_t)num) > memBytes){
				DEBUG_ShowMsg("DEBUG: Range exceeds configured memory (%lX bytes), cancelling.",(unsigned long)memBytes);
				return true;
			}
			DEBUG_ShowMsg("DEBUG: Created memory search instance.");
			MEMFINDInstance = new MEMFinder;
			MEMFINDInstance->baseLinear = (uint32_t)base;
		} else if ((MEMFINDInstance == NULL) && (num == 0)){
			DEBUG_ShowMsg("DEBUG: --MEMFIND-- Start memory search instance.");
			DEBUG_ShowMsg("DEBUG: Use MEMS to proceed through search instance.");
			DEBUG_ShowMsg("DEBUG: Usage: MEMFIND (!/L) [seg]:[offset] [range] (B/W/D)");
			DEBUG_ShowMsg("DEBUG: L - List entries matched in the search instance.");
			DEBUG_ShowMsg("DEBUG: ! - Cancel memory search instance.");
			return true;
		} else if (MEMFINDInstance != NULL){
			DEBUG_ShowMsg("DEBUG: Memory search instance is already active.");
			return true;
		}
		switch (*found){
			case 'B':
				DEBUG_ShowMsg("DEBUG: 1 byte specified.");
				MEMFINDInstance->size = 1;
				break;
			case 'W':
				DEBUG_ShowMsg("DEBUG: 2 bytes specified.");
				MEMFINDInstance->size = 2;
				break;
			case 'D':
				DEBUG_ShowMsg("DEBUG: 4 bytes specified.");
				MEMFINDInstance->size = 4;
				break;
			default:
				DEBUG_ShowMsg("DEBUG: No size specified, using 1 byte.");
				MEMFINDInstance->size = 1;
				break;
		}
		DEBUG_ShowMsg("DEBUG: RAM search from %04X:%04X (lin %08X) with range: %06X\n",seg,ofs,MEMFINDInstance->baseLinear,num);
		MEMFINDInstance->range = num;
		MEMFINDInstance->ofs = ofs;
		MEMFINDInstance->seg = seg;
		MEMFINDInstance->tableTruth.resize(num,true);
		for (uint32_t i = MEMFINDInstance->baseLinear; i < MEMFINDInstance->baseLinear + MEMFINDInstance->range; i++){
			mem_readb_checked((PhysPt)(i),&valfind8);
			MEMFINDInstance->tableValue.push_back(valfind8);
		}
		return true;
	}

	if (command == "MEMS") { // Search through MEMFIND for matching values in memory search instance
		bool parsed;
		uint32_t valfind;
		uint8_t valfind8;
		uint16_t valfind16;
		uint32_t valfind32;
		uint32_t value;
		uint32_t matches = 0;
		char opTypeStr[26];
		if (MEMFINDInstance == NULL){
			DEBUG_ShowMsg("DEBUG: MEMFIND Memory search is not active.\n");
			return true;
		}
		switch (found[0]){
			case '>':
				if (found[1] == '='){
					found+=2;
					MEMFINDInstance->opType = 4;
					sprintf(opTypeStr, "GREATER THAN OR EQUAL TO");
				} else {
					found+=1;
					MEMFINDInstance->opType = 1;
					sprintf(opTypeStr, "GREATER THAN");
				}
				break;
			case '<':
				if (found[1] == '='){
					found+=2;
					MEMFINDInstance->opType = 5;
					sprintf(opTypeStr, "LESS THAN OR EQUAL TO");
				} else {
					found+=1;
					MEMFINDInstance->opType = 2;
					sprintf(opTypeStr, "LESS THAN");
				}
				break;
			case '!':
				found+=1;
				MEMFINDInstance->opType = 3;
				sprintf(opTypeStr, "NOT EQUAL TO");
				break;
			case '=':
				found+=1;
				MEMFINDInstance->opType = 0;
				sprintf(opTypeStr, "EQUAL TO");
			default:
				MEMFINDInstance->opType = 0;
				sprintf(opTypeStr, "EQUAL TO");
				break;
		}
		SkipSpace(found);
		if (*found == ' '){
			DEBUG_ShowMsg("DEBUG: MEMS - Memory Search from MEMFIND instance.\n");
			DEBUG_ShowMsg("DEBUG: Usage: MEMS (OPERATOR) VALUE.\n");
			DEBUG_ShowMsg("DEBUG: Valid operators: >, >=, <, <=, !, =.\n");
			return true;
		} else if (*found == '\0'){
			DEBUG_ShowMsg("DEBUG: No value was entered. Comparing with previous values.\n");
			MEMFINDInstance->usePreviousValue = true;
			value = 0;
		} else {
			value = GetHexValue(found,found,&parsed);
			if (((value > 255) && (MEMFINDInstance->size == 1)) || ((value > 65535) && (MEMFINDInstance->size == 2))){
				DEBUG_ShowMsg("DEBUG: Memory search failed. Value is larger than specified size range.\n");
				return true;
			}
			MEMFINDInstance->value = value;
			MEMFINDInstance->usePreviousValue = false;
		}
		uint32_t y = 0;	//This index is seperated from the for loop, as the for loop can start from any index while this particular index starts at 0
		for (uint32_t i = MEMFINDInstance->baseLinear; i < MEMFINDInstance->baseLinear + MEMFINDInstance->range; i+=MEMFINDInstance->size){
			switch (MEMFINDInstance->size){
				case 1:
					mem_readb_checked((PhysPt)(i),&valfind8);
					valfind = valfind8;
					if (MEMFINDInstance->usePreviousValue == true){
						memcpy(&value, &MEMFINDInstance->tableValue[y], 1);
					}
					break;
				case 2:
					mem_readw_checked((PhysPt)(i),&valfind16);
					valfind = valfind16;
					if (MEMFINDInstance->usePreviousValue == true){
						memcpy(&value, &MEMFINDInstance->tableValue[y], 2);
					}
					break;
				case 4:
					mem_readd_checked((PhysPt)(i),&valfind32);
					valfind = valfind32;
					if (MEMFINDInstance->usePreviousValue == true){
						memcpy(&value, &MEMFINDInstance->tableValue[y], 4);
					}
					break;
				default:
					mem_readb_checked((PhysPt)(i),&valfind8);
					valfind = valfind8;
					if (MEMFINDInstance->usePreviousValue == true){
						memcpy(&value, &MEMFINDInstance->tableValue[y], 1);
					}
					break;
			}
			switch (MEMFINDInstance->opType){
				case 1:
					if ((valfind > value) && (MEMFINDInstance->tableTruth[y] == true)){
						matches++;
					} else {
						MEMFINDInstance->tableTruth[y] = false;
					}
					break;
				case 2:
					if ((valfind < value) && (MEMFINDInstance->tableTruth[y] == true)){
						matches++;
					} else {
						MEMFINDInstance->tableTruth[y] = false;
					}
					break;
				case 3:
					if ((valfind != value) && (MEMFINDInstance->tableTruth[y] == true)){
						matches++;
					} else {
						MEMFINDInstance->tableTruth[y] = false;
					}
					break;
				case 4:
					if ((valfind >= value) && (MEMFINDInstance->tableTruth[y] == true)){
						matches++;
					} else {
						MEMFINDInstance->tableTruth[y] = false;
					}
					break;
				case 5:
					if ((valfind <= value) && (MEMFINDInstance->tableTruth[y] == true)){
						matches++;
					} else {
						MEMFINDInstance->tableTruth[y] = false;
					}
					break;
				default:
					if ((valfind == value) && (MEMFINDInstance->tableTruth[y] == true)){
						matches++;
					} else {
						MEMFINDInstance->tableTruth[y] = false;
					}
					break;
			}
			y+=MEMFINDInstance->size;
		}
		MEMFINDInstance->matches = matches;
		MEMFINDInstance->iterations++;
		if (MEMFINDInstance->matches == 0){
			if (MEMFINDInstance->usePreviousValue == true){
				DEBUG_ShowMsg("DEBUG: No more matches found when comparing %s previous value. Memory search instance finished.\n",opTypeStr);
			} else {
				DEBUG_ShowMsg("DEBUG: No more matches found with value %s (%0*X). Memory search instance finished.\n",opTypeStr,MEMFINDInstance->size*2,value);
			}
			MEMFINDInstance->tableTruth.clear();
			MEMFINDInstance->tableValue.clear();
			MEMFINDInstance->tableTruth.shrink_to_fit();
			MEMFINDInstance->tableValue.shrink_to_fit();
			delete MEMFINDInstance;
			MEMFINDInstance = NULL;
		} else {
				if (MEMFINDInstance->usePreviousValue == true){
					DEBUG_ShowMsg("DEBUG: (%06X) addresses matching %s their previous values. Iterations: %06X\n",MEMFINDInstance->matches,opTypeStr,MEMFINDInstance->iterations);
				} else {
					DEBUG_ShowMsg("DEBUG: (%06X) matches found with value %s (%0*X). Iterations: %06X\n",MEMFINDInstance->matches,opTypeStr,MEMFINDInstance->size*2,value,MEMFINDInstance->iterations);
				}
		}
		return true;
	}

	if (command == "IV") { // Insert variable
		uint16_t seg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t ofs = GetHexValue(found,found); found++;
		char name[16];
		for (int i=0; i<16; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[15] = 0;

		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Created debug var %s at %04X:%04X\n",name,seg,ofs);
		CDebugVar::InsertVariable(name,(PhysPt)GetAddress(seg,ofs));
		return true;
	}

	if (command == "SV") { // Save variables
		char name[13];
		for (int i=0; i<12; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[12] = 0;
		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Variable list save (%s) : %s.\n",name,(CDebugVar::SaveVars(name)?"ok":"failure"));
		return true;
	}

	if (command == "LV") { // load variables
		char name[13];
		for (int i=0; i<12; i++) {
			if (found[i] && (found[i]!=' ')) name[i] = found[i];
			else { name[i] = 0; break; }
		}
		name[12] = 0;
		if(!name[0]) return false;
		DEBUG_ShowMsg("DEBUG: Variable list load (%s) : %s.\n",name,(CDebugVar::LoadVars(name)?"ok":"failure"));
		return true;
	}

    if (command == "EV") { // echo value (for viewing contents through GetHexValue
        std::string cpptmp;
        char tmp[128];
        bool parsed;

        SkipSpace(found);
        DEBUG_ShowMsg("EV of '%s' is:",found);

        while (*found) {
            uint32_t value = GetHexValue(found,found,&parsed); SkipSpace(found);
            if (!parsed) {
                DEBUG_ShowMsg("GetHexValue parse error at %s",found);
                break;
            }
            sprintf(tmp,"%lx",(unsigned long)value);
            if (!cpptmp.empty()) cpptmp += " ";
            cpptmp += tmp;
        }

        DEBUG_ShowMsg("%s",cpptmp.c_str());
        return true;
    }
	
	if (command == "ADDLOG") {
		if(found && *found)	DEBUG_ShowMsg("NOTICE: %s\n",found);
		return true;
	}

	if (command == "SR") { // Set register value
		DEBUG_ShowMsg("DEBUG: Set Register %s.\n",(ChangeRegister(found)?"success":"failure"));
		return true;
	}

	if (command == "SM") { // Set memory with following values
		uint16_t seg = (uint16_t)GetHexValue(found,found);
		if (*found == ':') { // allow seg:off syntax
			found++;
			SkipSpace(found);
		}
		uint32_t ofs = GetHexValue(found,found); SkipSpace(found);
		uint16_t count = 0;
		bool parsed;

		while (*found) {
			char prefix = 'B';
			uint32_t value;

			/* allow d: w: b: prefixes */
			if ((*found == 'B' || *found == 'W' || *found == 'D') && found[1] == ':') {
				prefix = *found; found += 2;
				value = GetHexValue(found,found,&parsed);
			}
			else {
				value = GetHexValue(found,found,&parsed);
			}

			SkipSpace(found);
			if (!parsed) {
				DEBUG_ShowMsg("GetHexValue parse error at %s",found);
				break;
			}

			if (prefix == 'D') {
				mem_writed_checked((PhysPt)GetAddress(seg,ofs+count),value);
				count += 4;
			}
			else if (prefix == 'W') {
				mem_writew_checked((PhysPt)GetAddress(seg,ofs+count),value);
				count += 2;
			}
			else if (prefix == 'B') {
				mem_writeb_checked((PhysPt)GetAddress(seg,ofs+count),value);
				count++;
			}
		}

		if (count > 0)
			DEBUG_ShowMsg("DEBUG: Memory changed (%u bytes)\n",(unsigned int)count);

		return true;
	}

	if (command == "SMV") { // Set memory with following values (virtual linear address)
		uint32_t ofs = GetHexValue(found,found); SkipSpace(found);
		uint16_t count = 0;
		bool parsed;

		while (*found) {
			char prefix = 'B';
			uint32_t value;

			/* allow d: w: b: prefixes */
			if ((*found == 'B' || *found == 'W' || *found == 'D') && found[1] == ':') {
				prefix = *found; found += 2;
				value = GetHexValue(found,found,&parsed);
			}
			else {
				value = GetHexValue(found,found,&parsed);
			}

			SkipSpace(found);
			if (!parsed) {
				DEBUG_ShowMsg("GetHexValue parse error at %s",found);
				break;
			}

			if (prefix == 'D') {
				mem_writed_checked((PhysPt)(ofs+count),value);
				count += 4;
			}
			else if (prefix == 'W') {
				mem_writew_checked((PhysPt)(ofs+count),value);
				count += 2;
			}
			else if (prefix == 'B') {
				mem_writeb_checked((PhysPt)(ofs+count),value);
				count++;
			}
		}

		if (count > 0)
			DEBUG_ShowMsg("DEBUG: Memory changed (%u bytes)\n",(unsigned int)count);

		return true;
	}

	if (command == "SMP") { // Set memory with following values (physical address)
		uint32_t ofs = GetHexValue(found,found); SkipSpace(found);
		uint16_t count = 0;
		bool parsed;

		while (*found) {
			char prefix = 'B';
			uint32_t value;

			/* allow d: w: b: prefixes */
			if ((*found == 'B' || *found == 'W' || *found == 'D') && found[1] == ':') {
				prefix = *found; found += 2;
				value = GetHexValue(found,found,&parsed);
			}
			else {
				value = GetHexValue(found,found,&parsed);
			}

			SkipSpace(found);
			if (!parsed) {
				DEBUG_ShowMsg("GetHexValue parse error at %s",found);
				break;
			}

			if (prefix == 'D') {
				physdev_writed((PhysPt)(ofs+count),value);
				count += 4;
			}
			else if (prefix == 'W') {
				physdev_writew((PhysPt)(ofs+count),value);
				count += 2;
			}
			else if (prefix == 'B') {
				physdev_writeb((PhysPt)(ofs+count),value);
				count++;
			}
		}

		if (count > 0)
			DEBUG_ShowMsg("DEBUG: Memory changed (%u bytes)\n",(unsigned int)count);

		return true;
	}

	if (command == "BPSYSENTER") {
		if (Toggle_BreakSYSEnter())
			DEBUG_ShowMsg("DEBUG: Breakpoint on SYSENTER set\n");
		else
			DEBUG_ShowMsg("DEBUG: Breakpoint on SYSENTER cleared\n");
		return true;
	}

	if (command == "BPSYSEXIT") {
		if (Toggle_BreakSYSExit())
			DEBUG_ShowMsg("DEBUG: Breakpoint on SYSEXIT set\n");
		else
			DEBUG_ShowMsg("DEBUG: Breakpoint on SYSEXIT cleared\n");
		return true;
	}

	if (command == "BP") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint::AddBreakpoint(seg,ofs,false);
		DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X\n",seg,ofs);
		return true;
	}

#if C_HEAVY_DEBUG

	if (command == "BPM") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint::AddMemBreakpoint(seg,ofs);
		DEBUG_ShowMsg("DEBUG: Set memory breakpoint at %04X:%04X\n",seg,ofs);
		return true;
	}

	if (command == "BPPM") { // Add new breakpoint
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(seg,ofs);
		if (bp)	{
			bp->SetType(BKPNT_MEMORY_PROT);
			DEBUG_ShowMsg("DEBUG: Set prot-mode memory breakpoint at %04X:%08X\n",seg,ofs);
		}
		return true;
	}

	if (command == "BPLM") { // Add new breakpoint
		uint32_t ofs = GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(0,ofs);
		if (bp) bp->SetType(BKPNT_MEMORY_LINEAR);
		DEBUG_ShowMsg("DEBUG: Set linear memory breakpoint at %08X\n",ofs);
		return true;
	}

#endif

	if (command == "BPINT") { // Add Interrupt Breakpoint
		uint8_t intNr	= (uint8_t)GetHexValue(found,found);
		bool all = !(*found);
		uint8_t valAH = (uint8_t)GetHexValue(found,found);
		if ((valAH==0x00) && (*found=='*' || all)) {
			CBreakpoint::AddIntBreakpoint(intNr,BPINT_ALL,BPINT_ALL,false);
			DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X\n",intNr);
		} else {
			all = !(*found);
			uint8_t valAL = (uint8_t)GetHexValue(found,found);
			if ((valAL==0x00) && (*found=='*' || all)) {
				CBreakpoint::AddIntBreakpoint(intNr,valAH,BPINT_ALL,false);
				DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X\n",intNr,valAH);
			} else {
				CBreakpoint::AddIntBreakpoint(intNr,valAH,valAL,false);
				DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X AL=%02X\n",intNr,valAH,valAL);
			}
		}
		return true;
	}

	if (command == "BPLIST") {
        DEBUG_BeginPagedContent();
		DEBUG_ShowMsg("Breakpoint list:\n");
		DEBUG_ShowMsg("-------------------------------------------------------------------------\n");
		CBreakpoint::ShowList();
        DEBUG_EndPagedContent();
		return true;
	}
    
    if (command == "FM") { // Freeze memory value at address
		uint16_t seg = (uint16_t)GetHexValue(found,found);found++; // skip ":"
		uint32_t ofs = GetHexValue(found,found);
        uint8_t value;
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(seg,ofs);
        Bitu address = (Bitu)GetAddress(bp->GetSegment(),bp->GetOffset());
		mem_readb_checked((PhysPt)address,&value);
        if (bp){ 
            bp->SetType(BKPNT_MEMORY_FREEZE);
            bp->SetValue(value);
        }
		DEBUG_ShowMsg("DEBUG: Set memory freeze at %04X:%04X with value: %02X\n",seg,ofs,value);
		return true;
	}

	if (command == "BPDEL") { // Delete Breakpoints
		uint8_t bpNr	= (uint8_t)GetHexValue(found,found); 
		if ((bpNr==0x00) && (*found=='*')) { // Delete all
			CBreakpoint::DeleteAll();
			Clear_SYSENTER_Debug();
			DEBUG_ShowMsg("DEBUG: Breakpoints deleted.\n");
		} else {
			// delete single breakpoint
			DEBUG_ShowMsg("DEBUG: Breakpoint deletion %s.\n",(CBreakpoint::DeleteByIndex(bpNr)?"success":"failure"));
		}
		return true;
	}

    const bool halt_on_retrace=(command == "VRT");
    if (halt_on_retrace) command = "RUN";

	if (command == "RUN")
        return DEBUG_ResumeNativeExecution(NativeDebug::Frontend::Interactive,false,halt_on_retrace,DEBUG_CommandSession());

	if (command == "RUNWATCH")
        return DEBUG_ResumeNativeExecution(NativeDebug::Frontend::Interactive,true,false,DEBUG_CommandSession());

	if (command == "STEPIN") {
        NativeDebug::Request request;
        request.frontend=NativeDebug::Frontend::Interactive;
        request.attachment=DEBUG_AttachNativeExecution(request.frontend,0);
        request.expected=DEBUG_NativeExecutionContext();
        if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted) {
            DEBUG_ShowMsg("DEBUG: STEPIN native execution admission refused\n");
            return false;
        }
        return true; // External bridge defers its wire reply until Poll(result).
	}

	if (command == "MEMREAD") {
		/* External bounded memory observation: returns the actual conventional
		 * RAM bytes through the debugger response. Read-only - it never executes
		 * a guest instruction, never writes guest memory or files, and refuses a
		 * request it cannot fully satisfy instead of substituting zero bytes. */
		if (!debugging || debug_running) {
			DEBUG_ShowMsg("DEBUG: MEMREAD requires the debugger to be stopped\n");
			return false;
		}
		if (cpu.pmode) {
			DEBUG_ShowMsg("DEBUG: MEMREAD supports real mode only\n");
			return false;
		}
		unsigned long seg = 0, ofs = 0, len = 0;
		int consumed = 0;
		char trailing = 0;
		if (sscanf(found, "%lx:%lx %lx%n", &seg, &ofs, &len, &consumed) != 3
				|| sscanf(found + consumed, " %c", &trailing) == 1
				|| seg > 0xFFFF || ofs > 0xFFFF || len < 1 || len > 256
				|| ofs + len > 0x10000) {
			DEBUG_ShowMsg("DEBUG: MEMREAD usage: MEMREAD <seg>:<off> <length 1-256> (hex)\n");
			return false;
		}
		unsigned long phys = seg * 16 + ofs;
		if (phys + len > 0xA0000) {
			DEBUG_ShowMsg("DEBUG: MEMREAD reads conventional RAM below A0000h only\n");
			return false;
		}
		char hex[2 * 256 + 1];
		for (unsigned long i = 0; i < len; i++) {
			uint8_t value = 0;
			if (mem_readb_checked((PhysPt)(phys + i), &value)) {
				DEBUG_ShowMsg("DEBUG: MEMREAD cannot read %04lX:%04lX\n", seg, ofs + i);
				return false;
			}
			snprintf(&hex[i * 2], 3, "%02X", (unsigned)value);
		}
		DEBUG_ShowMsg("DEBUG: MEMREAD %04lX:%04lX LEN=%04lX HEX=%s\n", seg, ofs, len, hex);
		return true;
	}

    if (command == "A20") {
        void MEM_A20_Enable(bool enabled);
        bool MEM_A20_Enabled(void);

        command.clear();
		stream >> command;

        if (command == "ON" || command == "1")
            MEM_A20_Enable(true);
        else if (command == "OFF" || command == "0")
            MEM_A20_Enable(false);
        else if (command == "")
            DEBUG_ShowMsg("A20 gate is %s",MEM_A20_Enabled() ? "ON" : "OFF");
        else
            return false;

        return true;
    }

    if (command == "PIC") { // interrupt controller state/controls
        void DEBUG_PICSignal(int irq,bool raise);
        void DEBUG_PICMask(int irq,bool mask);
        void DEBUG_PICAck(int irq);
        void DEBUG_LogPIC(void);

        DEBUG_BeginPagedContent();

        command.clear();
		stream >> command;

        if (command == "MASKIRQ") {
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICMask(irq,true);
        }
        else if (command == "UNMASKIRQ") {
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICMask(irq,false);
        }
        else if (command == "ACKIRQ") { /* debugging: manually acknowledge an IRQ where the DOS program failed to do so */
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICAck(irq);
        }
        else if (command == "LOWERIRQ") { /* manually lower an IRQ signal */
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICSignal(irq,false);
        }
        else if (command == "RAISEIRQ") { /* manually raise an IRQ signal */
            std::string what;
            stream >> what;
            int irq = atoi(what.c_str());
            DEBUG_PICSignal(irq,true);
        }
        else if (command == "") {
            DEBUG_LogPIC();
        }
        else {
            DEBUG_EndPagedContent();
            return false;
        }

        DEBUG_EndPagedContent();

        return true;
    }

	if (command == "C") { // Set code overview
		uint16_t codeSeg = (uint16_t)GetHexValue(found,found); found++;
		uint32_t codeOfs = GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Set code overview to %04X:%04X\n",codeSeg,codeOfs);
		codeViewData.useCS	= codeSeg;
		codeViewData.useEIP = codeOfs;
		codeViewData.cursorPos = 0;
		return true;
	}

	if (command == "D") { // Set data overview
		dataSeg = (uint16_t)GetHexValue(found,found); SkipSpace(found);
		if (*found == ':') { // allow seg:off syntax
			found++;
			SkipSpace(found);
		}
		dataOfs = GetHexValue(found,found); SkipSpace(found);
		dbg.set_data_view(DBGBlock::DATV_SEGMENTED);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X\n",dataSeg,dataOfs);
		return true;
	}

	if (command == "DV") { // Set data overview
		dataSeg = 0;
		dataOfs = GetHexValue(found,found); SkipSpace(found);
		dbg.set_data_view(DBGBlock::DATV_VIRTUAL);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X\n",dataSeg,dataOfs);
		return true;
	}

	if (command == "DP") { // Set data overview
		dataSeg = 0;
		dataOfs = GetHexValue(found,found); SkipSpace(found);
		dbg.set_data_view(DBGBlock::DATV_PHYSICAL);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X\n",dataSeg,dataOfs);
		return true;
	}

#if C_HEAVY_DEBUG

	if (command == "LOG" || command == "LOGS" || command == "LOGL" || command == "LOGC") {
		NativeDebug::Request request;
        request.attachment=DEBUG_CommandSession();
		request.expected=DEBUG_NativeExecutionContext();
		request.resume=true;
		request.log=true;
		request.log_type=command=="LOG" ? 1 : command=="LOGL" ? 2 : command=="LOGC" ? 3 : 0;
		DEBUG_ParseLogArgument(found,request.log_count);
		return DEBUG_RequestNativeExecution(request)==NativeDebug::Admission::Accepted;
	}

#endif

	if (command == "INTT") { //trace int.
		uint8_t intNr=0;
		if (!DEBUG_ParseInterruptArgument(found,intNr)) return false;
		NativeDebug::Request request;
        request.attachment=DEBUG_CommandSession();
		request.expected=DEBUG_NativeExecutionContext();
		request.action=NativeDebug::Request::Action::TraceInterrupt;
		request.interrupt=intNr;
		return DEBUG_RequestNativeExecution(request)==NativeDebug::Admission::Accepted;
	}

	if (command == "INT") { // start int.
		uint8_t intNr=0;
		if (!DEBUG_ParseInterruptArgument(found,intNr)) return false;
		NativeDebug::Request request;
        request.attachment=DEBUG_CommandSession();
		request.expected=DEBUG_NativeExecutionContext();
		request.action=NativeDebug::Request::Action::RunInterrupt;
		request.interrupt=intNr;
		request.resume=true;
		return DEBUG_RequestNativeExecution(request)==NativeDebug::Admission::Accepted;
	}

	if (command == "SELINFO") {
		while (found[0] == ' ') found++;
		char out1[200],out2[200];
		GetDescriptorInfo(found,out1,out2);
        DEBUG_ShowMsg("SelectorInfo %s:\n", found);
        DEBUG_ShowMsg("%s\n", out1);
        DEBUG_ShowMsg("%s\n", out2);
		return true;
	}

	if (command == "DOS") {
		stream >> command;
		if (command == "MCBS") LogMCBS();
		else if (command == "DEVS") LogDEVS();
		else if (command == "KERN") LogDOSKernMem();
		else if (command == "XMS") LogXMS();
#if !defined(OSFREE)
		else if (command == "EMS") LogEMS();
#endif
		else if (command == "FNKEY") LogFNKEY();
		else return false;

		return true;
	}

    if (command == "BIOS") {
        stream >> command;

        if (command == "MEM") LogBIOSMem();
        else return false;

        return true;
    }

    if (command == "MMX") { // MMX [=B|=W|=D|=Q|=S] [regindex] [command ...]
        while (*found == ' ') found++;

        char format = 'a'; /* internal: 'a' for "auto" */
        if (*found == '=') { /* =q as quads =d as dword =w as word =b as byte */
            found++;
            format = *found++;
            while (*found == ' ') found++;
        }

        int which = (*found != 0 && isxdigit(*found)) ? (int)strtol(found,&found,16) : -1; /* accept index as hexadecimal to be consistent with the rest of the debugger */
        while (*found == ' ') found++;

        /* is this an alternate command? */
        std::string subcommand;
        {
            char *start = found;
            while (*found != 0 && *found != ' ') found++;
            subcommand = std::string(start,(size_t)(found-start));
            while (*found == ' ') found++;
        }

	if (subcommand.empty()) {
            if (format == 'a') format = 'Q'; // =Q is default
            if (which >= 0 && which <= 7) {
                DEBUG_PrintMMX(which,format);
            }
            else {
                DEBUG_ShowMsg("MMX register contents:");
                for (which=0;which < 8;which++)
                    DEBUG_PrintMMX(which,format);
            }

            return true;
        }
        else if (subcommand == "SET") { /* remember this function capitalizes the entire string */
            if (which < 0 || which > 7) return false; // regindex is REQUIRED here
            // MMX [=B|=W|=D|=Q|=S] <regindex> SET [val,val,...]
            //
            // if you don't want to change a particular part of the MMX register, set val to nothing or ""
            // i.e. to set only the low 16 bits use =W 0 ,,,val
            static constexpr unsigned int param_max = 8;
            bool paramexist[param_max];
            bool paramfset[param_max];
            double paramf[param_max];
            uint64_t param[param_max];
            bool hasfloat=false;
            unsigned int pi=0;

            while (*found != 0 && pi < param_max) {
                if (*found == ',') {
                    found++;
                    paramexist[pi++] = false; // none specified, ok.
                    while (*found == ' ') found++;
                    continue; // skip space , space scan below
                }
                else if (found[0] == '\"' && found[1] == '\"') {
                    found += 2;
                    paramexist[pi++] = false; // none specified, ok.
                }
                else if (found[0] == 'M' && found[1] == 'M' && isxdigit(found[2])) { // i.e. "MM3", allow using other MMX registers as a value
                    found += 2;
                    int idx = strtol(found,&found,16);
                    if (idx >= 0 && idx <= 7) {
                        param[pi] = reg_mmx[idx]->q;
                        paramexist[pi] = true;
                        paramfset[pi] = false;
                        pi++;
                    }
                    else {
                        paramexist[pi] = false;
                        pi++;
                    }
                }
                else if (lookslikefloat(found)) {
                    paramf[pi] = strtod(found,&found);
                    paramexist[pi] = true;
                    paramfset[pi] = true;
                    hasfloat = true;
                    pi++;
                }
                else {
                    // FIXME: GetHexValue has a 32-bit datatype limit and therefore is unsuitable for 64-bit constants
                    bool parsed = false;
                    uint32_t r = GetHexValue(found,found,&parsed);
                    if (!parsed) return false;
                    param[pi] = (uint64_t)r;
                    paramexist[pi] = true;
                    paramfset[pi] = false;
                    pi++;
                }

                while (*found == ' ') found++;

                if (*found == ',') {
                    found++;
                    while (*found == ' ') found++;
                }
                else {
                    break; // I guess that's the end of the value
                }
            }

            /* if format wasn't specified, auto determine from number of parameters */
            if (format == 'a') {
                if (pi > 4) format = 'B';
                else if (pi > 2) format = 'W';
                else if (pi > 1) format = hasfloat ? 'S' : 'D';
                else format = 'Q';
            }

            if (pi == 0) {
                // um... I guess we're not setting anything, OK then.
                return true;
            }

            while (pi < param_max) paramexist[pi++] = false;

            // FIXME: This could be simple for() loops without copy-pasta if the MMX_reg struct had arrays
            if (format == 'B') {
                if (paramexist[0]) reg_mmx[which]->ub.b7 = param[0];
                if (paramexist[1]) reg_mmx[which]->ub.b6 = param[1];
                if (paramexist[2]) reg_mmx[which]->ub.b5 = param[2];
                if (paramexist[3]) reg_mmx[which]->ub.b4 = param[3];
                if (paramexist[4]) reg_mmx[which]->ub.b3 = param[4];
                if (paramexist[5]) reg_mmx[which]->ub.b2 = param[5];
                if (paramexist[6]) reg_mmx[which]->ub.b1 = param[6];
                if (paramexist[7]) reg_mmx[which]->ub.b0 = param[7];
            }
            else if (format == 'W') {
                if (paramexist[0]) reg_mmx[which]->uw.w3 = param[0];
                if (paramexist[1]) reg_mmx[which]->uw.w2 = param[1];
                if (paramexist[2]) reg_mmx[which]->uw.w1 = param[2];
                if (paramexist[3]) reg_mmx[which]->uw.w0 = param[3];
            }
            else if (format == 'D') {
                if (paramexist[0]) reg_mmx[which]->ud.d1 = param[0];
                if (paramexist[1]) reg_mmx[which]->ud.d0 = param[1];
            }
            else if (format == 'S') {
                if (paramexist[0]) {
                    if (paramfset[0]) *((float*)(&param[0])) = (float)paramf[0]; /* which becomes u32[] */
                    reg_mmx[which]->ud.d1 = param[0];
                }
                if (paramexist[1]) {
                    if (paramfset[1]) *((float*)(&param[1])) = (float)paramf[1]; /* which becomes u32[] */
                    reg_mmx[which]->ud.d0 = param[1];
                }
            }
            else { // 'Q'
                if (paramexist[0]) reg_mmx[which]->q = param[0];
            }

            DEBUG_PrintMMX(which,format);
            return true;
        }
    }

    // NTS: S = single precision float   F = double precision float   X = 128-bit unsigned integer constant
    if (command == "SSE") { // SSE [=B|=W|=D|=Q|=X|=S|=F] [regindex] [command ...]
        while (*found == ' ') found++;

        char format = 'a'; /* internal: 'a' for "auto" */
        if (*found == '=') { /* =q as quads =d as dword =w as word =b as byte */
            found++;
            format = *found++;
            while (*found == ' ') found++;
        }

        int which = (*found != 0 && isxdigit(*found)) ? (int)strtol(found,&found,16) : -1; /* accept index as hexadecimal to be consistent with the rest of the debugger */
        while (*found == ' ') found++;

        /* is this an alternate command? */
        std::string subcommand;
        {
            char *start = found;
            while (*found != 0 && *found != ' ') found++;
            subcommand = std::string(start,(size_t)(found-start));
            while (*found == ' ') found++;
        }

	if (subcommand.empty()) {
            if (format == 'a') format = 'S'; // =S is default (TODO: Wouldn't it be nice if XMM regs remembered the last datatype used with them so we can use that?)
            if (which >= 0 && which <= 7) {
                DEBUG_PrintSSE(which,format);
            }
            else {
                DEBUG_ShowMsg("SSE register contents:");
                for (which=0;which < 8;which++)
                    DEBUG_PrintSSE(which,format);
            }

            return true;
        }
        else if (subcommand == "SET") { /* remember this function capitalizes the entire string */
            if (which < 0 || which > 7) return false; // regindex is REQUIRED here
            // MMX [=B|=W|=D|=Q] <regindex> SET [val,val,...]
            //
            // if you don't want to change a particular part of the MMX register, set val to nothing or ""
            // i.e. to set only the low 16 bits use =W 0 ,,,val
            static constexpr unsigned int param_max = 16;
            bool paramexist[param_max];
            bool paramfset[param_max];
            double paramf[param_max];
            XMM_Reg param[param_max];
            bool hasfloat=false;
            unsigned int pi=0;

            while (*found != 0 && pi < param_max) {
                if (*found == ',') {
                    found++;
                    paramexist[pi++] = false; // none specified, ok.
                    while (*found == ' ') found++;
                    continue; // skip space , space scan below
                }
                else if (found[0] == '\"' && found[1] == '\"') {
                    found += 2;
                    paramexist[pi++] = false; // none specified, ok.
                }
                else if (found[0] == 'X' && found[1] == 'M' && found[2] == 'M' && isxdigit(found[3])) { // i.e. "XMM3", allow using other XMM registers as a value
                    found += 3;
                    int idx = strtol(found,&found,16);
                    if (idx >= 0 && idx <= 7) {
                        param[pi] = fpu.xmmreg[idx];
                        paramexist[pi] = true;
                        paramfset[pi] = false;
                        pi++;
                    }
                    else {
                        paramexist[pi] = false;
                        pi++;
                    }
                }
                else if (found[0] == 'M' && found[1] == 'M' && isxdigit(found[2])) { // i.e. "MM3", allow using other MMX registers as a value
                    found += 2;
                    int idx = strtol(found,&found,16);
                    if (idx >= 0 && idx <= 7) {
                        param[pi].u64[0] = reg_mmx[idx]->q;
                        param[pi].u64[1] = 0;
                        paramexist[pi] = true;
                        paramfset[pi] = false;
                        pi++;
                    }
                    else {
                        paramexist[pi] = false;
                        pi++;
                    }
                }
                else if (lookslikefloat(found)) {
                    paramf[pi] = strtod(found,&found);
                    paramexist[pi] = true;
                    paramfset[pi] = true;
                    hasfloat = true;
                    pi++;
                }
                else {
                    // FIXME: GetHexValue has a 32-bit datatype limit and therefore is unsuitable for 64-bit constants
                    bool parsed = false;
                    uint32_t r = GetHexValue(found,found,&parsed);
                    if (!parsed) return false;
                    param[pi].u32[0] = (uint64_t)r;
                    param[pi].u32[1] = 0;
                    param[pi].u32[2] = 0;
                    param[pi].u32[3] = 0;
                    paramexist[pi] = true;
                    paramfset[pi] = false;
                    pi++;
                }

                while (*found == ' ') found++;

                if (*found == ',') {
                    found++;
                    while (*found == ' ') found++;
                }
                else {
                    break; // I guess that's the end of the value
                }
            }

            /* if format wasn't specified, auto determine from number of parameters */
            if (format == 'a') {
                if (pi > 8) format = 'B';
                else if (pi > 4) format = 'W';
                else if (pi > 2) format = hasfloat ? 'S' : 'D';
                else if (pi > 1) format = hasfloat ? 'F' : 'Q';
                else format = 'X';
            }

            if (pi == 0) {
                // um... I guess we're not setting anything, OK then.
                return true;
            }

            while (pi < param_max) paramexist[pi++] = false;

            if (format == 'B') {
                for (unsigned int i=0;i < 16;i++) {
                    if (paramexist[i]) fpu.xmmreg[which].u8[15-i] = param[i].u8[0];
                }
            }
            else if (format == 'W') {
                for (unsigned int i=0;i < 8;i++) {
                    if (paramexist[i]) fpu.xmmreg[which].u16[7-i] = param[i].u16[0];
                }
            }
            else if (format == 'S') {
                for (unsigned int i=0;i < 4;i++) {
                    if (paramexist[i]) {
                        if (paramfset[i]) param[i].f32[0].v = (float)paramf[i]; /* which becomes u32[] */
                        fpu.xmmreg[which].u32[3-i] = param[i].u32[0];
                    }
                }
            }
            else if (format == 'D') {
                for (unsigned int i=0;i < 4;i++) {
                    if (paramexist[i]) fpu.xmmreg[which].u32[3-i] = param[i].u32[0];
                }
            }
            else if (format == 'F') {
                for (unsigned int i=0;i < 2;i++) {
                    if (paramexist[i]) {
                        if (paramfset[i]) param[i].f64[0].v = paramf[i]; /* which becomes u64[] */
                        fpu.xmmreg[which].u64[1-i] = param[i].u64[0];
                    }
                }
            }
            else if (format == 'Q') {
                for (unsigned int i=0;i < 2;i++) {
                    if (paramexist[i]) fpu.xmmreg[which].u64[1-i] = param[i].u64[0];
                }
            }
            else if (format == 'X') {
                if (paramexist[0]) fpu.xmmreg[which] = param[0];
            }

            DEBUG_PrintSSE(which,format);
            return true;
        }
    }

    if (command == "GUS") {
        std::string subcommand;
        {
            char *start = found;
            while (*found != 0 && *found != ' ') found++;
            subcommand = std::string(start,(size_t)(found-start));
            while (*found == ' ') found++;
        }

        if (subcommand.empty()) {
            DEBUG_BeginPagedContent();
            DEBUG_PrintGUS();
            DEBUG_EndPagedContent();
            return true;
        }
    }

    if (command == "VRD") {
        VGA_DebugRedraw();
        return true;
    }

    if (command == "RTC") {
        while (*found == ' ') found++;
        command.clear(); // iostream >> command does not update "command" if there is no string there, so it's basically fancy scanf() then!
        stream >> command;
        while (*found != 0 && *found != ' ') found++;
        while (*found == ' ') found++;

        if (command == "") {
            DEBUG_PrintRTC();
            return true;
        }
    }

    if (command == "VGA") {
        while (*found == ' ') found++;
        command.clear(); // iostream >> command does not update "command" if there is no string there, so it's basically fancy scanf() then!
        stream >> command;
        while (*found != 0 && *found != ' ') found++;
        while (*found == ' ') found++;

        if (IS_PC98_ARCH) {
            DEBUG_ShowMsg("VGA debugger commands not available in PC-98 mode");
            return false;
        }

	if (command == "DS") {
		std::string cmd2;
		while (*found == ' ') found++;
		stream >> cmd2;
		while (*found != 0 && *found != ' ') found++;
		while (*found == ' ') found++;

		if (cmd2 == "START") {
			bool sum = false;
			if (*found == '+') { sum = true; found++; }
			VGA_DebugOverrideStart(strtoul(found,NULL,16/*hexadecimal*/),sum);
			VGA_DebugRedraw();
		}
		else if (cmd2 == "X") {
			VGA_ResetDebugOverrides();
			VGA_DebugRedraw();
		}
		else {
			DEBUG_ShowMsg("Unknown VGA debug command");
			return false;
		}
	}
	else if (command == "FONTDUMP") { // Dump font RAM to file
		/* Rule: If the file extension is .BIN, write the entire contents of bitplane 2 where the font resides (64KB).
		 *       If the file extension is .BMP, write only the visible font characters in a neat 8x8 (256) matrix.
		 *       If you run this when EGA/VGA graphics are active, you will get "jibberish" based on the graphics on
		 *       screen, and that is your fault.
		 * TODO: VGA 512-char mode? */
		if (IS_PC98_ARCH) {
			LOG_MSG("Command not available in PC-98 mode");
			return false;
		}
		else if (!IS_EGAVGA_ARCH) {
			LOG_MSG("Command requires EGA, VGA, or SVGA machine type");
			return false;
		}
		else {
			std::string filename;
			const char *fext;

			while (*found == ' ') found++;
			while (*found != 0 && *found != ' ') filename += *found++;
			while (*found == ' ') found++;

			/* Possible parameters following the filename here */

			fext = (const char*)strrchr(filename.c_str(),'.');
			if (fext == NULL) fext = "";

			if (!strcasecmp(fext,".bin")) {
				VGA_DumpFontRamBIN(filename.c_str());
			}
			else if (!strcasecmp(fext,".bmp")) {
				VGA_DumpFontRamBMP(filename.c_str());
			}
			else {
				LOG_MSG("Unknown file extension '%s' in '%s'. Please use .BIN or .BMP",fext,filename.c_str());
				return false;
			}
		}
	}
	else if (command == "CRTC") {
            DEBUG_ShowMsg("VGA CRTC info: index=%02xh readonly=%u",vga.crtc.index,vga.crtc.read_only?1:0);
            DEBUG_ShowMsg("htotal=%02xh hdend=%02xh strhb=%02xh endhb=%02xh strrt=%02xh endrt=%02xh",
                vga.crtc.horizontal_total,              vga.crtc.horizontal_display_end,
                vga.crtc.start_horizontal_blanking,     vga.crtc.end_horizontal_blanking,
                vga.crtc.start_horizontal_retrace,      vga.crtc.end_horizontal_retrace);
            DEBUG_ShowMsg("vtotal=%02xh overflow=%02xh prerwscn=%02xh maxscnl=%02xh offset=%02xh",
                vga.crtc.vertical_total,                vga.crtc.overflow,
                vga.crtc.preset_row_scan,               vga.crtc.maximum_scan_line,
                vga.crtc.offset);
            DEBUG_ShowMsg("curs-st=%02xh curs-en=%02xh start-addr=%02x%02xh curs-loc=%02x%02xh",
                vga.crtc.cursor_start,                  vga.crtc.cursor_end,
                vga.crtc.start_address_high,            vga.crtc.start_address_low,
                vga.crtc.cursor_location_high,          vga.crtc.cursor_location_low);
            DEBUG_ShowMsg("strvrt=%02xh endvrt=%02xh vdend=%02xh underline=%02xh modectrl=%02xh",
                vga.crtc.vertical_retrace_start,        vga.crtc.vertical_retrace_end,
                vga.crtc.vertical_display_end,          vga.crtc.underline_location,
                vga.crtc.mode_control);
            DEBUG_ShowMsg("strvhb=%02xh endvhb=%02xh linecomp=%02xh",
                vga.crtc.start_vertical_blanking,       vga.crtc.end_vertical_blanking,
                vga.crtc.line_compare);
        }
        else if (command == "GC") {
            DEBUG_ShowMsg("VGA Graphics controller info: index=%02xh setreset=%02xh",vga.gfx.index,vga.gfx.set_reset);
            DEBUG_ShowMsg("enablesetreset=%02xh color-comp=%02xh data-rotate=%02xh rdmapsel=%02xh",
                vga.gfx.enable_set_reset,   vga.gfx.color_compare,  vga.gfx.data_rotate,
                vga.gfx.read_map_select);
            DEBUG_ShowMsg("mode=%02xh misc=%02xh color-dont-care=%02xh bitmask=%02xh",
                vga.gfx.mode,               vga.gfx.miscellaneous,  vga.gfx.color_dont_care,
                vga.gfx.bit_mask);
        }
        else if (command == "DAC") {
            // FIXME: Remove this var, use vga.dac.bits == 8 instead!
            extern bool vga_8bit_dac;

            DEBUG_ShowMsg("VGA DAC info: bits=%u pel_mask=%02xh pel_index[RGB]=%02xh",
                vga_8bit_dac ? 8 : 6,//FIXME
                vga.dac.pel_mask,       vga.dac.pel_index);
            DEBUG_ShowMsg("state=%02xh write_index=%02xh read_index=%02xh first_changed=%u",
                vga.dac.state,          vga.dac.write_index,
                vga.dac.read_index,     (unsigned int)vga.dac.first_changed);
            DEBUG_ShowMsg("hidac_counter=%u reg02=%02xh",
                vga.dac.hidac_counter,  vga.dac.reg02);
        }
        else if (command == "DACPAL") {
            std::string cpptmp;
            char tmp[64];

            DEBUG_BeginPagedContent();

            DEBUG_ShowMsg("VGA DAC palette (RGB):");
            for (unsigned int i=0;i < 256;i += 8) {
                sprintf(tmp,"%02x: ",i);
                cpptmp = tmp;

                for (unsigned int c=0;c < 8;c++) {
                    sprintf(tmp,"%02x%02x%02x%c",
                        vga.dac.rgb[i+c].red,
                        vga.dac.rgb[i+c].green,
                        vga.dac.rgb[i+c].blue,
                        c == 3 ? '-' : ' '
                    );
                    cpptmp += tmp;
                }

                DEBUG_ShowMsg("%s",cpptmp.c_str());
            }

            DEBUG_EndPagedContent();
        }
        else if (command == "SEQ") {
            DEBUG_ShowMsg("VGA Sequencer info: index=%02xh",vga.seq.index);
            DEBUG_ShowMsg("reset=%02xh clockmode=%02xh map_mask=%02xh charmapsel=%02xh memmode=%02xh",
                vga.seq.reset,      vga.seq.clocking_mode,      vga.seq.map_mask,
                vga.seq.character_map_select,                   vga.seq.memory_mode);
        }
        else if (command == "AC") {
            std::string cpptmp;
            char tmp[64];

            DEBUG_ShowMsg("VGA Attribute Controller info: attrindexmode=%u",vga.internal.attrindex?1:0);
            DEBUG_ShowMsg("mode_control=%02xh horz-pel-pan=%02xh overscan-color=%02xh",
                vga.attr.mode_control,  vga.attr.horizontal_pel_panning,
                vga.attr.overscan_color);
            DEBUG_ShowMsg("color-plane-en=%02xh color-select=%02xh index=%02xh",
                vga.attr.color_plane_enable,    vga.attr.color_select,  vga.attr.index);
            DEBUG_ShowMsg("disabled-by-index=%u disabled-by-idx1-bit5=%u index-written=%u",
                (vga.attr.disabled & 1) ? 1 : 0,
                (vga.attr.disabled & 2) ? 1 : 0,
                vga.internal.attrindex);

            cpptmp = " ";
            for (unsigned int i=0;i < 16;i++) {
                sprintf(tmp,"%02x%c",vga.attr.palette[i],((i&3) == 3 && i != 15) ? '-' : ' ');
                cpptmp += tmp;
            }

            DEBUG_ShowMsg("palette={%s}",cpptmp.c_str());
        }
        else if (command == "DRAW") {
            DEBUG_ShowMsg("VGA draw info: resizing=%u width=%lu height=%lu blocks=%lu",
                vga.draw.resizing?1:0,(unsigned long)vga.draw.width,(unsigned long)vga.draw.height,(unsigned long)vga.draw.blocks);
            DEBUG_ShowMsg("address=%lxh panning=%lu bytes-skip=%lu lin-mask=%lx pln-mask=%lx",
                (unsigned long)vga.draw.address,(unsigned long)vga.draw.panning,(unsigned long)vga.draw.bytes_skip,
                (unsigned long)vga.draw.linear_mask,(unsigned long)vga.draw.planar_mask);
            DEBUG_ShowMsg("addr-add=%lxh line-length=%lu addrline-total=%lu addrline=%lu",
                (unsigned long)vga.draw.address_add,(unsigned long)vga.draw.line_length,
                (unsigned long)vga.draw.address_line_total,(unsigned long)vga.draw.address_line);
            DEBUG_ShowMsg("line-total=%lu vblank-skip=%lu lines-done=%lu split-line=%lu",
                (unsigned long)vga.draw.lines_total,(unsigned long)vga.draw.vblank_skip,
                (unsigned long)vga.draw.lines_done,(unsigned long)vga.draw.split_line);
            DEBUG_ShowMsg("byte-pan-shft=%lu render-stop=%lu render-max=%lu scrn-ratio=%.3f",
                (unsigned long)vga.draw.byte_panning_shift,(unsigned long)vga.draw.render_step,
                (unsigned long)vga.draw.render_max,vga.draw.screen_ratio);
            DEBUG_ShowMsg("blinking=%lu blink=%u char9dot=%u has-split=%u vret-trig=%u",
                (unsigned long)vga.draw.blinking,vga.draw.blink?1:0,
                vga.draw.char9dot?1:0,vga.draw.has_split?1:0,
                vga.draw.vret_triggered?1:0);
            DEBUG_ShowMsg("bpp=%lu curs-addr=%lxh curs-s=%u curs-e=%u curs-c=%u curs-d=%u curs-en=%u",
                (unsigned long)vga.draw.bpp,
                (unsigned long)vga.draw.cursor.address,
                vga.draw.cursor.sline,
                vga.draw.cursor.eline,
                vga.draw.cursor.count,
                vga.draw.cursor.delay,
                vga.draw.cursor.enabled);
// TODO: Remove vga.draw.refresh, appears to be unused
        }
        else if (command == "MODE") {
            DEBUG_ShowMsg("VGA mode info:");
            DEBUG_ShowMsg("mode=%s vref=%.3fHz href=%.3fHz chrclk=%.3fHz dotclk=%.3fHz",
                mode_texts[vga.mode],1000.0/vga.draw.delay.vtotal,1000.0/vga.draw.delay.htotal,
                vga.draw.clock,vga.draw.oscclock);
            DEBUG_ShowMsg("disp-start=%lxh real-start=%lxh retrace=%u scanlen=%lu cursor-start=%lxh",
                (unsigned long)vga.config.display_start,
                (unsigned long)vga.config.real_start,
                vga.config.retrace?1:0,
                (unsigned long)vga.config.scan_len,
                (unsigned long)vga.config.cursor_start);
            DEBUG_ShowMsg("line-compare=%lu chained=%u compat-chain4=%u pel-pan=%u hline-skip=%u",
                (unsigned long)vga.config.line_compare,
                vga.config.chained,
                (unsigned int)vga.config.compatible_chain4,
                (unsigned int)vga.config.pel_panning,
                (unsigned int)vga.config.hlines_skip);
            DEBUG_ShowMsg("byte-skip=%u addr-shift=%u rd-mode=%u wr-mode=%u rdmap-sel=%u",
                (unsigned int)vga.config.bytes_skip,
                (unsigned int)vga.config.addr_shift,
                (unsigned int)vga.config.read_mode,
                (unsigned int)vga.config.write_mode,
                (unsigned int)vga.config.read_map_select);
            DEBUG_ShowMsg("col-dont-care=%u color-compare=%u data-rotate=%u raster-op=%02xh",
                (unsigned int)vga.config.color_dont_care,
                (unsigned int)vga.config.color_compare,
                (unsigned int)vga.config.data_rotate,
                (unsigned int)vga.config.raster_op);
            DEBUG_ShowMsg("fbmsk=%x fmmsk=%x fnmmsk=%x fsr=%x fnesr=%x fesr=%x feasr=%x",
                vga.config.full_bit_mask,
                vga.config.full_map_mask,
                vga.config.full_not_map_mask,
                vga.config.full_set_reset,
                vga.config.full_not_enable_set_reset,
                vga.config.full_enable_set_reset,
                vga.config.full_enable_and_set_reset);
	    {
		    std::string s;

		    if (vga.complexity.flags & VGACMPLX_MAP_MASK) s += "mapmask|";
		    if (vga.complexity.flags & VGACMPLX_NON_EXTENDED) s += "nonextend|";
		    if (vga.complexity.flags & VGACMPLX_ODDEVEN) s += "oddeven|";
		    if (vga.complexity.flags & VGACMPLX_BITMASK) s += "bitmask|";
		    if (vga.complexity.flags & VGACMPLX_COLORDONTCARE) s += "clrdontcare|";
		    if (vga.complexity.flags & VGACMPLX_WRITEMODE) s += "wrmode|";
		    if (vga.complexity.flags & VGACMPLX_READMODE) s += "rdmode|";
		    if (vga.complexity.flags & VGACMPLX_ROPROT) s += "roprot|";
		    if (vga.complexity.flags & VGACMPLX_SETRESET) s += "setreset|";
		    DEBUG_ShowMsg("complexity-flags=0x%lx=%s",(unsigned long)vga.complexity.flags,s.c_str());
	    }
        }
        else {
            return false;
        }

        return true;
   }

    if (command == "PC98") {
        stream >> command;

        if (!IS_PC98_ARCH) {
            DEBUG_ShowMsg("PC-98 debugger commands not available except in PC-98 mode");
            return false;
        }

        if (command == "MODE") {
            DEBUG_ShowMsg("Mode info:");
            DEBUG_ShowMsg("mode=%s vref=%.3fHz href=%.3fHz chrclk=%.3fHz dotclk=%.3fHz",
                mode_texts[vga.mode],1000.0/vga.draw.delay.vtotal,1000.0/vga.draw.delay.htotal,
                vga.draw.clock,vga.draw.oscclock);
        }
        else if (command == "DACPAL") {
            std::string cpptmp;
            char tmp[64];

            DEBUG_BeginPagedContent();

            cpptmp = "Current analog palette index: ";
            sprintf(tmp,"%02xh\n",pc98_16col_analog_rgb_palette_index);
            cpptmp += tmp;

            DEBUG_ShowMsg("Digital 8-color DAC palette (RGB):");
            for (unsigned int i=0;i < 8;i += 8) {
                sprintf(tmp,"%02x: ",i);
                cpptmp = tmp;

                for (unsigned int c=0;c < 8;c++) {
                    sprintf(tmp,"%01x",pc98_pal_digital[i+c]);
                    cpptmp += tmp;
                }

                DEBUG_ShowMsg("%s",cpptmp.c_str());
            }

            DEBUG_ShowMsg("Analog 16-color DAC palette (RGB):");
            for (unsigned int i=0;i < 16;i += 8) {
                sprintf(tmp,"%02x: ",i);
                cpptmp = tmp;

                for (unsigned int c=0;c < 8;c++) {
                    sprintf(tmp,"%01x%01x%01x%c",
                        pc98_pal_analog[((i+c)*3)+1], /* R */
                        pc98_pal_analog[((i+c)*3)+0], /* G */
                        pc98_pal_analog[((i+c)*3)+2], /* B */
                        c == 3 ? '-' : ' '
                    );
                    cpptmp += tmp;
                }

                DEBUG_ShowMsg("%s",cpptmp.c_str());
            }

            DEBUG_ShowMsg("VGA DAC palette (RGB):");
            for (unsigned int i=0;i < 256;i += 8) {
                sprintf(tmp,"%02x: ",i);
                cpptmp = tmp;

                for (unsigned int c=0;c < 8;c++) {
                    sprintf(tmp,"%02x%02x%02x%c",
                        pc98_pal_vga[((i+c)*3)+1], /* R */
                        pc98_pal_vga[((i+c)*3)+0], /* G */
                        pc98_pal_vga[((i+c)*3)+2], /* B */
                        c == 3 ? '-' : ' '
                    );
                    cpptmp += tmp;
                }

                DEBUG_ShowMsg("%s",cpptmp.c_str());
            }

            DEBUG_EndPagedContent();
        }
        else if (command == "GRAPHICS") {
            const auto &gdc = pc98_gdc[GDC_SLAVE];
            std::string cpptmp;

            cpptmp.clear();
            if (gdc_analog) {
                if (pc98_gdc_vramop & (1 << VOPBIT_VGA))
                    cpptmp += "'256-color packed' ";
                else
                    cpptmp += "'16-color planar' ";
            }
            else {
                cpptmp += "'8-color planar' ";
            }

            if (egc_enable_enable) /* Port 6Ah, 0x06/0x07 to enable EGC enable commands 0x04/0x05 */
                cpptmp += "EGC-ENABL ";
            if (pc98_gdc_vramop & (1 << VOPBIT_EGC)) /* Port 6Ah, 0x04/0x05 */
                cpptmp += "EGC ";
            if (pc98_gdc_vramop & (2 << VOPBIT_GRCG)) /* Port 7Ch, bits [7:6]. bit 7 (2 << VOPBIT_GRCG) is the enable. */
                cpptmp += "GRCG ";

            if (gdc.display_enable)
                cpptmp += "ENABLED ";
            else
                cpptmp += "DISABLED ";

            if (gdc.doublescan) {
                if (pc98_graphics_hide_odd_raster_200line)
                    cpptmp += "200-LINE-EVEN-SCAN ";
                else
                    cpptmp += "DOUBLESCAN ";
            }

            if (pc98_pegc_linear_framebuffer_enabled())
                cpptmp += "PEGC-LFB ";

            if (pc98_gdc_vramop & (1 << VOPBIT_PEGC_PLANAR))
                cpptmp += "256-PLANAR ";

            DEBUG_ShowMsg("PC-98 graphics mode: %s",cpptmp.c_str());

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 page display: cpu=%u display-pending=%u display-active=%u pitch=%u",
                    (pc98_gdc_vramop & (1 << VOPBIT_ACCESS))?1:0,
                    GDC_display_plane_pending,
                    GDC_display_plane,
                    gdc.display_pitch);

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 status: gdc5mhz=%u vsync-int-trig=%u rowheight=%u lines-drawn=%u",
                gdc_5mhz_mode,GDC_vsync_interrupt,gdc.row_height,(unsigned int)vga.draw.lines_done);
            DEBUG_ShowMsg("  cur-row-line=%u cur-scan=0x%x cur-partition=%u/%u part-remline=%u adw=%u",
                gdc.row_line,gdc.scan_address,gdc.display_partition,gdc.display_partition_mask+1,gdc.display_partition_rem_lines,gdc.active_display_words_per_line);
            DEBUG_ShowMsg("  vram-bound=%uKB monomode=%u",
                pc98_256kb_boundary?256:128,pc98_monochrome_mode);

            /*--------------------*/

            cpptmp.clear();
            {
                char tmp[16];
                for (unsigned int i=0;i < 16;i++) {
                    sprintf(tmp,"%02x%c",gdc.param_ram[i],((i&3) == 3 && i != 15) ? '-' : ' ');
                    cpptmp += tmp;
                }
            }
            DEBUG_ShowMsg("PC-98 GDC PRAM: wptr=%u %s",gdc.param_ram_wptr,cpptmp.c_str());

            /*---------------------*/
            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 GRCG: active=%u mode=%s tilecount=%u modereg(7Ch)=%02xh",
                (pc98_gdc_vramop & (2 << VOPBIT_GRCG))?1:0,
                (pc98_gdc_vramop & (1 << VOPBIT_GRCG))?"Read/Modify/Write":"TCR/TDW",
                pc98_gdc_tile_counter,
                pc98_gdc_modereg);

            /*---------------------*/
            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 GRCG tiles: [%02x %02x] [%02x %02x] [%02x %02x] [%02x %02x]",
                pc98_gdc_tiles[0].b[0],
                pc98_gdc_tiles[0].b[1],
                pc98_gdc_tiles[1].b[0],
                pc98_gdc_tiles[1].b[1],
                pc98_gdc_tiles[2].b[0],
                pc98_gdc_tiles[2].b[1],
                pc98_gdc_tiles[3].b[0],
                pc98_gdc_tiles[3].b[1]);
        }
        else if (command == "TEXT") {
            const auto &gdc = pc98_gdc[GDC_MASTER];
            std::string cpptmp;

            cpptmp.clear();
            if (gdc.display_enable)
                cpptmp += "ENABLED ";
            else
                cpptmp += "DISABLED ";

            if (gdc.doublescan)
                cpptmp += "DOUBLESCAN ";

            if (pc98_attr4_graphic)
                cpptmp += "SIMPLE-GRPH ";
            else
                cpptmp += "VERT-LINE ";

            DEBUG_ShowMsg("PC-98 text mode: %s",cpptmp.c_str());

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 display/cursor: pitch=%u curs-en=%u curs-blink=%u curs-blinkrate=%u",
                gdc.display_pitch,
                gdc.cursor_enable,
                gdc.cursor_blink,
                gdc.cursor_blink_rate);

            DEBUG_ShowMsg("  curs-blink-count=%u curs-blink-state=%u/4 crt-mode=%u",
                gdc.cursor_blink_count,
                gdc.cursor_blink_state,
                pc98_crt_mode?1:0);

            /*--------------------*/

            cpptmp.clear();
            DEBUG_ShowMsg("PC-98 status: gdc5mhz=%u vsync-int-trig=%u rowheight=%u lines-drawn=%u pitch=%u",
                gdc_5mhz_mode,GDC_vsync_interrupt,gdc.row_height,(unsigned int)vga.draw.lines_done,gdc.display_pitch);
            DEBUG_ShowMsg("  cur-row-line=%u cur-scan=0x%x cur-partition=%u/%u part-remline=%u adw=%u",
                gdc.row_line,gdc.scan_address,gdc.display_partition,gdc.display_partition_mask+1,gdc.display_partition_rem_lines,gdc.active_display_words_per_line);

            /*--------------------*/

            cpptmp.clear();
            {
                char tmp[16];
                for (unsigned int i=0;i < 16;i++) {
                    sprintf(tmp,"%02x%c",gdc.param_ram[i],((i&3) == 3 && i != 15) ? '-' : ' ');
                    cpptmp += tmp;
                }
            }
            DEBUG_ShowMsg("PC-98 GDC PRAM: wptr=%u %s",gdc.param_ram_wptr,cpptmp.c_str());

            /*--------------------*/

            DEBUG_ShowMsg("PC-98 CG raster: row-scan=[start=%u/32 end-incl=%u/32] blank-at-in-charcell=%u/32",
                    pc98_text_first_row_scanline_start,
                    pc98_text_first_row_scanline_end,
                    pc98_text_row_scanline_blank_at);

            DEBUG_ShowMsg("PC-98 CG scrollregion: countstart=%u char-rows=%u lines=%u",
                    pc98_text_row_scroll_count_start,
                    pc98_text_row_scroll_num_lines,
                    pc98_text_row_scroll_lines);
        }
        else if (command == "CGIO") {
            DEBUG_ShowMsg("PC-98 CG I/O port state (A1h-A9h odd): char-row=%u char-code=0x%04x",
                    a1_font_char_offset,
                    a1_font_load_addr);
        }
        else if (command == "EGC") {
            DEBUG_ShowMsg("PC-98 EGC Raw registers:");
            DEBUG_ShowMsg("  4A0h even: %04xh %04xh %04xh %04xh",
                pc98_egc_raw_values[0],
                pc98_egc_raw_values[1],
                pc98_egc_raw_values[2],
                pc98_egc_raw_values[3]);
            DEBUG_ShowMsg("  4A8h even: %04xh %04xh %04xh %04xh bitplane-access=%02xh",
                pc98_egc_raw_values[4],
                pc98_egc_raw_values[5],
                pc98_egc_raw_values[6],
                pc98_egc_raw_values[7],
                pc98_egc_access);
            DEBUG_ShowMsg("  bg-color=%02xh bgcm=[%02xh %02xh] [%02xh %02xh] [%02xh %02xh] [%02xh %02xh]",
                pc98_egc_background_color,
                pc98_egc_bgcm[0].b[0],
                pc98_egc_bgcm[0].b[1],
                pc98_egc_bgcm[1].b[0],
                pc98_egc_bgcm[1].b[1],
                pc98_egc_bgcm[2].b[0],
                pc98_egc_bgcm[2].b[1],
                pc98_egc_bgcm[3].b[0],
                pc98_egc_bgcm[3].b[1]);
            DEBUG_ShowMsg("  fg-color=%02xh fgcm=[%02xh %02xh] [%02xh %02xh] [%02xh %02xh] [%02xh %02xh]",
                pc98_egc_foreground_color,
                pc98_egc_fgcm[0].b[0],
                pc98_egc_fgcm[0].b[1],
                pc98_egc_fgcm[1].b[0],
                pc98_egc_fgcm[1].b[1],
                pc98_egc_fgcm[2].b[0],
                pc98_egc_fgcm[2].b[1],
                pc98_egc_fgcm[3].b[0],
                pc98_egc_fgcm[3].b[1]);
            DEBUG_ShowMsg("  fgc-select=%d=%s lead-plane=%u srcmask=[%02xh %02xh]",
                pc98_egc_fgc,
                egc_fgc_modes[pc98_egc_fgc],
                pc98_egc_lead_plane,
                pc98_egc_srcmask[0],pc98_egc_srcmask[1]);
            DEBUG_ShowMsg("  mask=[%02xh %02xh] mask-effective=[%02xh %02xh] ROP=%02xh",
                pc98_egc_mask[0],pc98_egc_mask[1],
                pc98_egc_maskef[0],pc98_egc_maskef[1],
                pc98_egc_rop);
            DEBUG_ShowMsg("  compare-lead-plane=%u lightsource=%u shiftinput=%u regload=%u",
                pc98_egc_compare_lead,
                pc98_egc_lightsource,
                pc98_egc_shiftinput,
                pc98_egc_regload);
            DEBUG_ShowMsg("  shift-desc=%u shf-destbit=%u shf-srcbit=%u shf-length=%u",
                pc98_egc_shift_descend,
                pc98_egc_shift_destbit,
                pc98_egc_shift_srcbit,
                pc98_egc_shift_length);
            DEBUG_ShowMsg("  %s",
                pc98_egc_shift_debug_status().c_str());
        }
        else {
            return false;
        }

        return true;
    }

    if (command == "EMU") {
        stream >> command;

        if (command == "MEM") LogEMUMem();
        else if (command == "MACHINE") LogEMUMachine();
        else return false;

        return true;
    }

    if (command == "CALLBACKS") {
        DEBUG_BeginPagedContent();

        void DBG_CALLBACK_Dump(void);
        DBG_CALLBACK_Dump();

        DEBUG_EndPagedContent();
        return true;
    }

    if (command == "INP" || command == "INB") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint8_t r = IO_ReadB(port);
        DEBUG_ShowMsg("Result: %x",(unsigned int)r);
        return true;
    }

    if (command == "INW") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint16_t r = IO_ReadW(port);
        DEBUG_ShowMsg("Result: %x",(unsigned int)r);
        return true;
    }

    if (command == "IND") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint32_t r = IO_ReadD(port);
        DEBUG_ShowMsg("Result: %x",(unsigned int)r);
        return true;
    }

    if (command == "OUTP" || command == "OUTB") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint8_t r = (uint8_t)GetHexValue(found,found);
        IO_WriteB(port,r);
        return true;
    }

    if (command == "OUTW") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint16_t r = (uint16_t)GetHexValue(found,found);
        IO_WriteW(port,r);
        return true;
    }

    if (command == "OUTD") {
        uint16_t port = (uint16_t)GetHexValue(found,found);
        uint32_t r = GetHexValue(found,found);
        IO_WriteD(port,r);
        return true;
    }

	if (command == "GDT") {LogGDT(); return true;}

	if (command == "LDT") {LogLDT(); return true;}

	if (command == "IDT") {LogIDT(); return true;}

	if (command == "PAGING") {LogPages(found); return true;}

	if (command == "CPU") {LogCPUInfo(); return true;}

	if (command == "FPU") {LogFPUInfo(); return true;}

	if (command == "INTVEC") {
		if (found[0] != 0) {
			OutputVecTable(found);
			return true;
		}
	}

	if (command == "INTHAND") {
		if (found[0] != 0) {
			uint8_t intNr = (uint8_t)GetHexValue(found,found);
			DEBUG_ShowMsg("DEBUG: Set code overview to interrupt handler %X\n",intNr);
            if (cpu.pmode) {
                Descriptor gate;
                if (cpu.idt.GetDescriptor((Bitu)intNr<<3u,gate)) {
                    codeViewData.useCS	= (uint16_t)gate.GetSelector();
                    codeViewData.useEIP = (uint32_t)gate.GetOffset();
                }
                else {
                    DEBUG_ShowMsg("INTHAND unable to retrieve vector");
                }
            }
            else {
                codeViewData.useCS	= mem_readw(intNr*4u+2u);
                codeViewData.useEIP = mem_readw(intNr*4u);
            }
			codeViewData.cursorPos = 0;
			return true;
		}
	}

	if(command == "EXTEND") { //Toggle additional data.
		showExtend = !showExtend;
		return true;
	}

	if(command == "TIMERIRQ") { //Start a timer irq
		DEBUG_RaiseTimerIrq(); 
		DEBUG_ShowMsg("Debug: Timer Int started.\n");
		return true;
	}

	if(command == "TIME") { //Current time
		command.clear();
		stream >> command;
        char c=dos.tables.country[13], c1, c2;
        uint32_t hour, min, sec;
        if (command != "" && sscanf(command.c_str(),"%u%c%u%c%u",&hour,&c1,&min,&c2,&sec) == 5 && c1==c && c2==c && hour < 24 && min < 60 && sec < 60) {
            uint32_t ticks=(uint32_t)(((double)(hour*3600+min*60+sec+0.2))*18.206481481);
            mem_writed(BIOS_TIMER,ticks);
        }
        uint32_t ticks=mem_readd(BIOS_TIMER);
        uint8_t add=mem_readb(BIOS_24_HOURS_FLAG);
        mem_writeb(BIOS_24_HOURS_FLAG,0); // reset the "flag"
        uint16_t cx=(uint16_t)(ticks >> 16u), dx=(uint16_t)(ticks & 0xffff);
        if (add) DOS_AddDays(add);
        ticks=((Bitu)cx<<16)|dx;
        Bitu time=(Bitu)((100.0/((double)PIT_TICK_RATE/65536.0)) * (double)ticks);
        time/=100;
        sec=(uint8_t)((Bitu)time % 60); // seconds
        time/=60;
        min=(uint8_t)((Bitu)time % 60); // minutes
        time/=60;
        hour=(uint8_t)((Bitu)time % 24); // hours
        DEBUG_ShowMsg("Internal time: %u%c%02u%c%02u\n",hour,c,min,c,sec);
		return true;
	}

	if(command == "DATE") { //Current date
		command.clear();
		stream >> command;
        char c=dos.tables.country[11], c1, c2;
        uint32_t year,month,day;
        int n = dos.tables.country[0]==1?sscanf(command.c_str(),"%u%c%u%c%u",&day,&c1,&month,&c2,&year):(dos.tables.country[0]==2?sscanf(command.c_str(),"%u%c%u%c%u",&year,&c1,&month,&c2,&day):sscanf(command.c_str(),"%u%c%u%c%u",&month,&c1,&day,&c2,&year));
        if (command != "" && n == 5 && c1==c && c2==c && year>=1980 && month && month<=12 && day && (day<=GetMonthDays(month)||(month==2&&year%4==0&&day==29))) {
            dos.date.year=year;
            dos.date.month=month;
            dos.date.day=day;
        }
//      uint32_t ticks=mem_readd(BIOS_TIMER); // unused
        uint8_t add=mem_readb(BIOS_24_HOURS_FLAG);
        mem_writeb(BIOS_24_HOURS_FLAG,0); // reset the "flag"
        if (add) DOS_AddDays(add);
        DEBUG_ShowMsg("Internal date: %s\n", FormatDate(dos.date.year, dos.date.month, dos.date.day));
		return true;
	}

#if C_HEAVY_DEBUG
	if (command == "HEAVYLOG") { // Create Cpu log file
		logHeavy = !logHeavy;
		DEBUG_ShowMsg("DEBUG: Heavy cpu logging %s.\n",logHeavy?"on":"off");
		return true;
	}

	if (command == "ZEROPROTECT") { //toggle zero protection
		zeroProtect = !zeroProtect;
		DEBUG_ShowMsg("DEBUG: Zero code execution protection %s.\n",zeroProtect?"on":"off");
		return true;
	}

#endif
	if (command == "HELP" || command == "?") {
		DEBUG_BeginPagedContent();
		DEBUG_ShowMsg("Debugger commands (enter all values in hex or as register):\n");
		DEBUG_ShowMsg("Commands------------------------------------------------\n");
		DEBUG_ShowMsg("BP     [segment]:[offset] - Set breakpoint.\n");
		DEBUG_ShowMsg("BPINT  [intNr] *          - Set interrupt breakpoint.\n");
		DEBUG_ShowMsg("BPINT  [intNr] [ah] *     - Set interrupt breakpoint with ah.\n");
		DEBUG_ShowMsg("BPINT  [intNr] [ah] [al]  - Set interrupt breakpoint with ah and al.\n");
#if C_HEAVY_DEBUG
		DEBUG_ShowMsg("BPM    [segment]:[offset] - Set memory breakpoint (memory change).\n");
		DEBUG_ShowMsg("BPPM   [selector]:[offset]- Set pmode-memory breakpoint (memory change).\n");
		DEBUG_ShowMsg("BPLM   [linear address]   - Set linear memory breakpoint (memory change).\n");
#endif
		DEBUG_ShowMsg("BPLIST                    - List breakpoints.\n");
		DEBUG_ShowMsg("BPDEL  [bpNr] / *         - Delete breakpoint nr / all.\n");
		DEBUG_ShowMsg("C / D  [segment]:[offset] - Set code / data view address.\n");
		DEBUG_ShowMsg("DV [addr]                 - Set data view to linear (virtual) address\n");
		DEBUG_ShowMsg("DP [addr]                 - Set data view to physical address\n");
		DEBUG_ShowMsg("DOS MCBS                  - Show Memory Control Block chain.\n");
		DEBUG_ShowMsg("DOS KERN                  - Show DOS kernel memory blocks.\n");
		DEBUG_ShowMsg("DOS XMS                   - Show XMS memory handles.\n");
		DEBUG_ShowMsg("DOS EMS                   - Show EMS memory handles.\n");
		DEBUG_ShowMsg("DOS FNKEY                 - Show PC-98 FnKey mapping.\n");
		DEBUG_ShowMsg("BIOS MEM                  - Show BIOS memory blocks.\n");
		DEBUG_ShowMsg("CALLBACKS                 - Show callbacks of interrupts.\n");
		DEBUG_ShowMsg("INT [nr] / INTT [nr]      - Execute / Trace into interrupt.\n");
		DEBUG_ShowMsg("MEMREAD <seg>:<off> <len> - Read 1-256 conventional RAM bytes (external).\n");
		DEBUG_ShowMsg("STEPIN                    - Execute exactly one instruction, then stop.\n");
#if C_HEAVY_DEBUG
		DEBUG_ShowMsg("LOG [num]                 - Write CPU log file.\n");
		DEBUG_ShowMsg("LOGS/LOGL/LOGC [num]      - Write short/long/cs:ip-only CPU log file.\n");
		DEBUG_ShowMsg("HEAVYLOG                  - Enable/Disable automatic CPU log when DOSBox-X exits.\n");
		DEBUG_ShowMsg("ZEROPROTECT               - Enable/Disable zero code execution detection.\n");
#endif
		DEBUG_ShowMsg("ADDLOG [message]          - Add message to the log file.\n");
		DEBUG_ShowMsg("SR [reg] [value]          - Set register value. Multiple pairs allowed.\n");
		DEBUG_ShowMsg("SM [seg]:[off] [val] [.]..- Set memory with following values.\n");
		DEBUG_ShowMsg("SMV [addr] [val] [.]..    - Set memory with following values at linear (virtual) address.\n");
		DEBUG_ShowMsg("FM [seg]:[off]            - Freeze memory value at address.\n");
		DEBUG_ShowMsg("EV [value [value] ...]    - Show register value(s).\n");
		DEBUG_ShowMsg("IV [seg]:[off] [name]     - Create var name for memory address.\n");
		DEBUG_ShowMsg("SV [filename]             - Save var list in file.\n");
		DEBUG_ShowMsg("LV [filename]             - Load var list from file.\n");

		DEBUG_ShowMsg("VRD                       - Redraw video.\n");
		DEBUG_ShowMsg("VGA cmd                   - VGA related debugging commands.\n");
		DEBUG_ShowMsg("PC98 cmd                  - PC98 related debugging commands.\n");
		DEBUG_ShowMsg("EMU MEM/MACHINE           - Show emulator memory or machine info.\n");
		DEBUG_ShowMsg("MEMDUMP seg:off len [filename] - Write memory to a text file (default: MEMDUMP.TXT).\n");
		DEBUG_ShowMsg("MEMDUMPBIN seg:off len [filename] - Write memory to a binary file (default: MEMDUMP.BIN).\n");
        DEBUG_ShowMsg("MEMFIND [seg]:[off] [.].. - Start memory find search instance.\n");
		DEBUG_ShowMsg("MEMS [operator] [value]   - Search value within instance.\n");
		DEBUG_ShowMsg("SELINFO [segName]         - Show selector info.\n");

		DEBUG_ShowMsg("INTVEC [filename]         - Writes interrupt vector table to file.\n");
		DEBUG_ShowMsg("INTHAND [intNum]          - Set code view to interrupt handler.\n");

		DEBUG_ShowMsg("MMX [=t] [reg]            - Display MMX register file (t can be B,W,D,Q)\n");
		DEBUG_ShowMsg("MMX [=t] [reg] SET [val]  - Set MMX register (t can be B,W,D,Q), val is comma-separated\n");
		DEBUG_ShowMsg("SSE [=t] [reg]            - Display SSE register file (t can be B,W,D,Q,X,S,F)\n");
		DEBUG_ShowMsg("SSE [=t] [reg] SET [val]  - Set SSE register (t can be B,W,D,Q,X,S,F), val is comma-separated\n");
		DEBUG_ShowMsg("CPU                       - Display CPU status information.\n");
		DEBUG_ShowMsg("FPU                       - Display FPU status information.\n");
		DEBUG_ShowMsg("GDT                       - Lists descriptors of the GDT.\n");
		DEBUG_ShowMsg("LDT                       - Lists descriptors of the LDT.\n");
		DEBUG_ShowMsg("IDT                       - Lists descriptors of the IDT.\n");
		DEBUG_ShowMsg("PAGING [page]             - Display content of page table.\n");
		DEBUG_ShowMsg("EXTEND                    - Toggle additional info.\n");
		DEBUG_ShowMsg("TIMERIRQ                  - Run the system timer.\n");
		DEBUG_ShowMsg("TIME [time]               - Display or change the internal time.\n");
		DEBUG_ShowMsg("DATE [date]               - Display or change the internal date.\n");
		DEBUG_ShowMsg("VRT                       - Run, then enter debugger at next vertical retrace.\n");

		DEBUG_ShowMsg("IN[P|W|D] [port]          - I/O port read byte/word/dword.\n");
		DEBUG_ShowMsg("OUT[P|W|D] [port] [data]  - I/O port write byte/word/dword.\n");

		DEBUG_ShowMsg("LUA expr/chunk | LUAFILE path | LUARESET - Persistent trusted Interactive Lua (when compiled).\n");
		DEBUG_ShowMsg("HELP                      - Help\n");
		DEBUG_ShowMsg("Keys------------------------------------------------\n");
		DEBUG_ShowMsg("F3/F6                     - Previous command in history.\n");
		DEBUG_ShowMsg("F4/F7                     - Next command in history.\n");
		DEBUG_ShowMsg("F5                        - Run.\n");
		DEBUG_ShowMsg("F8                        - Toggle printable characters.\n");
		DEBUG_ShowMsg("F9                        - Set/Remove breakpoint.\n");
		DEBUG_ShowMsg("F10/F11                   - Step over / trace into instruction.\n");
		DEBUG_ShowMsg("ALT + D/E/S/X/B           - Set data view to DS:SI/ES:DI/SS:SP/DS:DX/ES:BX.\n");
		DEBUG_ShowMsg("Escape                    - Clear input line.");
		DEBUG_ShowMsg("Up/Down                   - Scroll up/down in the current window.\n");
		DEBUG_ShowMsg("Page Up/Down              - Page up/down in the current window.\n");
		DEBUG_ShowMsg("Home/End                  - Move to begin/end of the current window.\n");
		DEBUG_ShowMsg("TAB/Shift+TAB             - Select next/prev window\n");
		DEBUG_EndPagedContent();

		return true;
	}

	if (recognized) *recognized = false;
	return false;
}

bool ParseCommand(char* str)
{
    return ParseCommandWithResult(str, nullptr);
}

bool DEBUG_ParseInterruptArgument(const char *argument, uint8_t &interrupt)
{
    std::vector<char> text(argument,argument+strlen(argument));
    text.push_back(0);
    upcase(text.data());
    char *end=text.data();
    while (*end==' ' || *end=='\t') ++end;
    if (!*end) return false;
    bool parsed=false;
    const auto value=GetHexValue(end,end,&parsed);
    while (*end==' ' || *end=='\t') ++end;
    if (!parsed || *end) return false;
    // Preserve the debugger parser's byte conversion, including expressions.
    interrupt=static_cast<uint8_t>(value);
    return true;
}

#if C_HEAVY_DEBUG
void DEBUG_ParseLogArgument(const char *argument, int &count)
{
    // Preserve the debugger's existing hexadecimal expression/count semantics.
    std::string text=argument ? argument : "";
    for (auto &c:text) c=toupper(static_cast<unsigned char>(c));
    text.push_back('\0');
    char *cursor=&text[0];
    count=static_cast<int>(GetHexValue(cursor,cursor));
}
#endif

bool DEBUG_ExecuteCommand(const char* command)
{
	if (command == NULL || *command == 0) {
		DEBUG_ShowMsg("*** Debugger command not recognized");
		return false;
	}

	std::vector<char> buffer(command, command + strlen(command));
	buffer.push_back(0);

	bool recognized = false;
	if (ParseCommandWithResult(buffer.data(), &recognized))
		return true;

	if (!recognized) DEBUG_ShowMsg("*** Debugger command not recognized");
	return false;
}

bool DEBUG_ExecuteSessionCommand(const char* command, NativeDebug::Attachment attachment)
{
    if (!attachment.serial) return false;
    const auto previous=command_attachment;
    struct Restore {
        NativeDebug::Attachment previous;
        ~Restore(){command_attachment=previous;}
    } restore{previous};
    command_attachment=attachment;
    return DEBUG_ExecuteCommand(command);
}

char* AnalyzeInstruction(char* inst, bool saveSelector) {
	static char result[256];

	char instu[256];

	strcpy(instu,inst);
	upcase(instu);

	result[0] = 0;
	char* pos = strchr(instu,'[');
	if (pos) {
		char prefix[3];
		uint16_t seg;
		// Segment prefix ?
		if (*(pos-1)==':') {
			char* segpos = pos-3;
			prefix[0] = tolower(*segpos);
			prefix[1] = tolower(*(segpos+1));
			prefix[2] = 0;
			seg = (uint16_t)GetHexValue(segpos,segpos);
		} else {
			if (strstr(pos,"SP") || strstr(pos,"BP")) {
				seg = SegValue(ss);
				strcpy(prefix,"ss");
			} else {
				seg = SegValue(ds);
				strcpy(prefix,"ds");
			}
		}

		pos++;
		uint32_t adr = GetHexValue(pos,pos);
		while (*pos!=']') {
			if (*pos=='+') {
				pos++;
				adr += GetHexValue(pos,pos);
			} else if (*pos=='-') {
				pos++;
				adr -= GetHexValue(pos,pos);
			} else
				pos++;
		}
		if (Is16BitSegment(seg))
			adr &= 0xffffu;
		const uint64_t address64 = GetAddress(seg,adr);
		const uint32_t address = (uint32_t)address64;
		if (address64 == mem_no_address) {
			sprintf(result,"[illegal]");
		}
		else {
			static char outmask[] = "%s:[%04X]=%02X";
			bool illegal = false;

			if (cpu.pmode) outmask[6] = '8';
			switch (DasmLastOperandSize()) {
			case 8: {
				uint8_t val = 0;
				illegal = mem_readb_checked(address,&val);
				if (!illegal) {
					outmask[12] = '2';
					sprintf(result,outmask,prefix,adr,val);
				}
			} break;
			case 16: {
				uint16_t val = 0;
				illegal = mem_readw_checked(address,&val);
				if (!illegal) {
					outmask[12] = '4';
					sprintf(result,outmask,prefix,adr,val);
				}
			} break;
			case 32: {
				uint32_t val = 0;
				illegal = mem_readd_checked(address,&val);
				if (!illegal) {
					outmask[12] = '8';
					sprintf(result,outmask,prefix,adr,val);
				}
			} break;
			default:
				illegal = true;
				break;
			}

			if (illegal)
				sprintf(result,"[illegal]");
		}
		// Variable found ?
		CDebugVar* var = (address64 != mem_no_address) ? CDebugVar::FindVar(address) : NULL;
		if (var) {
			// Replace occurrence
			char* pos1 = strchr(inst,'[');
			char* pos2 = strchr(inst,']');
			if (pos1 && pos2) {
				char temp[256];
				strcpy(temp,pos2);				// save end
				pos1++; *pos1 = 0;				// cut after '['
				strcat(inst,var->GetName());	// add var name
				strcat(inst,temp);				// add end
			}
		}
		// show descriptor info, if available
		if ((cpu.pmode) && saveSelector) {
			strcpy(curSelectorName,prefix);
		}
	}
	// If it is a callback add additional info
	pos = strstr(inst,"callback");
	if (pos) {
		pos += 9;
		Bitu nr = GetHexValue(pos,pos);
		const char* descr = CALLBACK_GetDescription(nr);
		if (descr) {
			strcat(inst,"  ("); strcat(inst,descr); strcat(inst,")");
		}
	}
	// Must be a jump
	if (instu[0] == 'J')
	{
		bool jmp = false;
		switch (instu[1]) {
		case 'A' :	{	jmp = (get_CF()?false:true) && (get_ZF()?false:true); // JA
					}	break;
		case 'B' :	{	if (instu[2] == 'E') {
							jmp = (get_CF()?true:false) || (get_ZF()?true:false); // JBE
						} else {
							jmp = get_CF()?true:false; // JB
						}
					}	break;
		case 'C' :	{	if (instu[2] == 'X') {
							jmp = reg_cx == 0; // JCXZ
						} else {
							jmp = get_CF()?true:false; // JC
						}
					}	break;
		case 'E' :	{	jmp = get_ZF()?true:false; // JE
					}	break;
		case 'G' :	{	if (instu[2] == 'E') {
							jmp = (get_SF()?true:false)==(get_OF()?true:false); // JGE
						} else {
							jmp = (get_ZF()?false:true) && ((get_SF()?true:false)==(get_OF()?true:false)); // JG
						}
					}	break;
		case 'L' :	{	if (instu[2] == 'E') {
							jmp = (get_ZF()?true:false) || ((get_SF()?true:false)!=(get_OF()?true:false)); // JLE
						} else {
							jmp = (get_SF()?true:false)!=(get_OF()?true:false); // JL
						}
					}	break;
		case 'M' :	{	jmp = true; // JMP
					}	break;
		case 'N' :	{	switch (instu[2]) {
						case 'B' :	
						case 'C' :	{	jmp = get_CF()?false:true;	// JNB / JNC
									}	break;
						case 'E' :	{	jmp = get_ZF()?false:true;	// JNE
									}	break;
						case 'O' :	{	jmp = get_OF()?false:true;	// JNO
									}	break;
						case 'P' :	{	jmp = get_PF()?false:true;	// JNP
									}	break;
						case 'S' :	{	jmp = get_SF()?false:true;	// JNS
									}	break;
						case 'Z' :	{	jmp = get_ZF()?false:true;	// JNZ
									}	break;
						}
					}	break;
		case 'O' :	{	jmp = get_OF()?true:false; // JO
					}	break;
		case 'P' :	{	if (instu[2] == 'O') {
							jmp = get_PF()?false:true; // JPO
						} else {
							jmp = get_SF()?true:false; // JP / JPE
						}
					}	break;
		case 'S' :	{	jmp = get_SF()?true:false; // JS
					}	break;
		case 'Z' :	{	jmp = get_ZF()?true:false; // JZ
					}	break;
		}
		if (jmp) {
			pos = strchr(instu,'$');
			if (pos) {
				pos = strchr(instu,'+');
				if (pos) {
					strcpy(result,"(down)");
				} else {
					strcpy(result,"(up)");
				}
			}
		} else {
			sprintf(result,"(no jmp)");
		}
	}
	return result;
}

// data window
void win_data_ui_down(int count) {
    if (count > 0)
        dataOfs += (unsigned)count * 16;
}

void win_data_ui_up(int count) {
    if (count > 0)
        dataOfs -= (unsigned)count * 16;
}

// code window
void win_code_ui_down(int count) {
    if (dbg.win_code != NULL) {
        int y,x;

        getmaxyx(dbg.win_code,y,x);

        while (count-- > 0) {
            if (codeViewData.cursorPos < (y-1)) codeViewData.cursorPos++;
            else codeViewData.useEIP += codeViewData.firstInstSize;
        }
    }
}

void win_code_ui_up(int count) {
    if (dbg.win_code != NULL) {
        int y,x;

        getmaxyx(dbg.win_code,y,x);

        (void)y; // SET, BUT UNUSED

        while (count-- > 0) {
            if (codeViewData.cursorPos>0)
                codeViewData.cursorPos--;
            else {
                uint32_t newEIP = codeViewData.useEIP - 1;
                if(codeViewData.useEIP) {
                    Bitu bytes = 0;
                    char dline[200];
                    for (; bytes < 10; bytes++) {
                        PhysPt start = (PhysPt)GetAddress(codeViewData.useCS,newEIP);
                        Bitu size = DasmI386(dline, start, newEIP, cpu.code.big);
                        if(codeViewData.useEIP == newEIP+size) break;
                        newEIP--;
                    }
                    if (bytes>=10) newEIP = codeViewData.useEIP - 1;
                }
                codeViewData.useEIP = newEIP;
            }
        }
    }
}

#ifdef WIN32
extern "C" INPUT_RECORD * _pdcurses_hax_inputrecord(void);
#endif

/* NTS: DOSBox SVN and almost (or all?) forks of DOSBox
 *      zero CPU_Cycles and CPU_CyclesLeft, which is a
 *      perfectly fine way to enter the debugger, but
 *      a side effect of this is that emulator time is
 *      effectively jumped forward to the start of the
 *      next 1ms tick. If you're debugging time-dependent
 *      code this has the effect of code that magically
 *      works when you debug it but crashes normally, or
 *      code that magically breaks when you debug it but
 *      runs fine otherwise.
 *
 *      See include/pic.h for how these variables affect
 *      emulator time used everywhere else in this code,
 *      specifically PIC_TickIndex() and PIC_FullIndex(). */
int32_t DEBUG_Run(int32_t amount,bool quickexit) {
    if (!NativeControl::diagnosticEffectAllowed()) return 0;
    // Also guard native/manual stepping after a Lua decoder return was lost.
    if (DEBUG_ContinuationUnavailable()) return 0;
#if C_DEBUG_LUA
    // Native authorized step/continue releases the current terminal receipt;
    // automatic predicate suppression is never a terminal release.
    if(!Debugger_PredicateHeld())predicate_summary.terminal_valid=false;
#endif
	skipFirstInstruction = true;
	CPU_CycleLeft += CPU_Cycles - amount;
	CPU_Cycles = amount;
	int32_t ret = (int32_t)DOSBOX_DispatchDecoder();
	if (quickexit) SetCodeWinStart();
	else {
		// ensure all breakpoints are activated
		CBreakpoint::ActivateBreakpoints();
		DOSBOX_SetNormalLoop();
	}
	return ret;
}

#ifdef WIN32
/* Translate VT escape sequences into ncurses KEY_* constants. Needed because
   we set ENABLE_VIRTUAL_TERMINAL_INPUT on the debugger console input handle
   (so the terminal host stops swallowing F11 etc. for fullscreen). With that
   flag, function keys arrive as VT sequences (e.g. F11 = ESC [ 23 ~) instead of
   virtual key codes, so ncurses' getch() returns them char by char and the
   KEY_F(N) cases below never fire. */
static int dbg_getch_vt(void) {
	int c = getch();
	if (c != 27) return c;

	int c2 = getch();
	if (c2 < 0) return 27; /* plain ESC */

	if (c2 == 'O') {
		/* SS3: ESC O X */
		int c3 = getch();
		switch (c3) {
			case 'P': return KEY_F(1);
			case 'Q': return KEY_F(2);
			case 'R': return KEY_F(3);
			case 'S': return KEY_F(4);
			case 'A': return KEY_UP;
			case 'B': return KEY_DOWN;
			case 'C': return KEY_RIGHT;
			case 'D': return KEY_LEFT;
			case 'H': return KEY_HOME;
			case 'F': return KEY_END;
		}
		return 27;
	}

	if (c2 == '[') {
		/* CSI: ESC [ [params] final */
		int param = 0;
		int c3 = getch();
		while (c3 >= '0' && c3 <= '9') {
			param = param * 10 + (c3 - '0');
			c3 = getch();
		}
		if (c3 == '~') {
			switch (param) {
				case 1: return KEY_HOME;
				case 2: return KEY_IC;
				case 3: return KEY_DC;
				case 4: return KEY_END;
				case 5: return KEY_PPAGE;
				case 6: return KEY_NPAGE;
				case 11: return KEY_F(1);
				case 12: return KEY_F(2);
				case 13: return KEY_F(3);
				case 14: return KEY_F(4);
				case 15: return KEY_F(5);
				case 17: return KEY_F(6);
				case 18: return KEY_F(7);
				case 19: return KEY_F(8);
				case 20: return KEY_F(9);
				case 21: return KEY_F(10);
				case 23: return KEY_F(11);
				case 24: return KEY_F(12);
			}
		} else if (param == 0) {
			switch (c3) {
				case 'A': return KEY_UP;
				case 'B': return KEY_DOWN;
				case 'C': return KEY_RIGHT;
				case 'D': return KEY_LEFT;
				case 'H': return KEY_HOME;
				case 'F': return KEY_END;
			}
		}
		return 27;
	}

	/* Alt+letter or other ESC-prefixed sequence — push back so the existing
	   case 27 handler below can read it as before. */
	ungetch(c2);
	return 27;
}
#endif
// Native execution-result tail shared by debugger continuation paths.
// Retain decoder callbacks here; only the RunMachine return seam consumes them
// after command/evaluator/service unwind. Negative loop results pass through.
static NativeDebug::Execution native_execution;
NativeDebug::Attachment DEBUG_AttachNativeExecution(NativeDebug::Frontend frontend,uint64_t principal);
bool DEBUG_DetachNativeExecution(NativeDebug::Attachment attachment);
// Selected DEBUGBOX launch only. Nested EXEC and foreign termination cannot
// establish absence of this target. The shell return is the cleanup fence.
static bool agent_target_launch=false, agent_target_exited=false;
static uint16_t agent_target_psp=0;
static uint64_t agent_target_owner=0;
void DEBUG_AgentBeginTarget(void) {
    agent_target_launch=true;agent_target_exited=false;
    agent_target_psp=0;agent_target_owner=0;
}
void DEBUG_AgentTargetTerminated(uint16_t psp) {
    if (agent_target_psp && psp==agent_target_psp &&
        DOSBOX_MachineServiceOwner().token==agent_target_owner)
        agent_target_exited=true;
}
bool DEBUG_AgentEndTarget(void) {
    const bool exited=agent_target_exited;
    agent_target_launch=false;
    if (exited) {
        const auto attachment=DEBUG_AttachNativeExecution(NativeDebug::Frontend::Agent,0);
        if (!DEBUG_DetachNativeExecution(attachment)) return false;
        agent_target_psp=0;agent_target_owner=0;
    }
    return exited;
}
uint64_t DEBUG_NativeExecutionOperation(void) {return native_execution.operation();}
// Terminal projection is synchronous and optional, never ownership or delivery
// acknowledgement. Native cleanup has already run and the slot is immutable.
static void DEBUG_ProjectNativeTerminal(NativeDebug::Attachment attachment) {
#if defined(C_DOSBOX_AGENT)
    const auto *terminal=native_execution.result(attachment);
    if (terminal && terminal->frontend==NativeDebug::Frontend::Agent)
        dosbox_agent::AGENT_NotifyDebuggerStopped(terminal->cs,terminal->ip,terminal);
#else
    (void)attachment;
#endif
}
#if C_HEAVY_DEBUG
static void DEBUG_CleanupSessionInstrumentation();
#endif
void DEBUG_NativeDecoderEntered(void) {
    const auto *request=native_execution.activeRequest();
    if (request && request->frontend==NativeDebug::Frontend::Lua &&
        request->completion==NativeDebug::Request::Completion::NativeBoundary &&
        !request->resume) {
        // The original native continuation enters this decoder, never held
        // service. Match DEBUG_Run(1) after PIC allocated the slice, retaining
        // unused cycles for subsequent execution.
        CPU_CycleLeft+=CPU_Cycles-1;
        CPU_Cycles=1;
    }
    native_execution.decoderEntered();
}
static uint64_t dispatch_operation=0;
unsigned DEBUG_EnterNativeDecoderFlags(void) {
    const unsigned saved=(inhibit_int_breakpoint?1u:0u) |
                         (mustCompleteInstruction?2u:0u);
    if (const auto *request=native_execution.activeRequest()) {
        // Ordinary RUN's initial skip is scoped to its dispatcher decoder,
        // not every subsequent normal-loop slice. Otherwise a retained RUN
        // would inhibit its own execution breakpoint forever.
        const bool local=request->frontend==NativeDebug::Frontend::Lua &&
                         request->completion==NativeDebug::Request::Completion::NativeBoundary;
        if (request->action!=NativeDebug::Request::Action::Phase &&
            (local || dispatch_operation==native_execution.operation())) {
            inhibit_int_breakpoint=request->resume && !local;
            mustCompleteInstruction=true;
        }
    }
    return saved;
}
bool DEBUG_NativeDecoderRestoresCore(void) {
    const auto *request=native_execution.activeRequest();
    return request && request->action!=NativeDebug::Request::Action::Phase && request->watch;
}
void DEBUG_LeaveNativeDecoderFlags(unsigned saved) {
    inhibit_int_breakpoint=(saved&1u)!=0;
    mustCompleteInstruction=(saved&2u)!=0;
}
// Cleanup belongs to the exact active request, not the frontend projection.
static void DEBUG_CleanupNativeExecution(const NativeDebug::Request &request) {
    for(auto id:native_condition_breakpoints)CBreakpoint::DeleteInternalTemporaryById(id,nullptr);
    native_condition_breakpoints.clear();
    if (native_execution_breakpoint) {
        CBreakpoint::DeleteInternalTemporaryById(native_execution_breakpoint,nullptr);
        native_execution_breakpoint=0;
    }
#if C_HEAVY_DEBUG
    if (request.log && cpuLog) {
        cpuLogFile.flush();cpuLogFile.close();cpuLog=false;cpuLogCounter=0;
    }
#else
    (void)request;
#endif
}
static void DEBUG_ExecutionOwnerDeparting(uint64_t token) {
#if C_DEBUG_LUA
    LocalDriverOwnerDeparting(token);
#endif
    const auto *request=native_execution.activeRequest();
    if (!request) request=native_execution.pendingRequest();
    if (request && request->expected.invocation==token &&
        !(native_execution.state()==NativeDebug::State::Active &&
          request->frontend==NativeDebug::Frontend::Lua &&
          request->completion==NativeDebug::Request::Completion::NativeBoundary))
        DEBUG_CleanupNativeExecution(*request);
    const auto attachment=request?request->attachment:NativeDebug::Attachment{};
    if (token==agent_target_owner && agent_target_exited)
        native_execution.settle(native_execution.decoderCalls(),false,0,0,0,0,0,0,NativeDebug::Terminal::ProgramExit);
    native_execution.ownerDeparting(token);
    DEBUG_ProjectNativeTerminal(attachment);
}

static bool native_phase_executing=false;
static DebuggerPhaseResult native_phase_result={};
static uint64_t native_phase_operation=0;
bool DEBUG_NativePhaseExecuting() { return native_phase_executing; }
const DebuggerPhaseResult *DEBUG_NativePhaseResult(NativeDebug::Attachment attachment, uint64_t operation) {
    const auto *result=native_execution.result(attachment);
    return result && result->operation==operation && native_phase_operation==operation ? &native_phase_result : nullptr;
}
bool DEBUG_NativeExecutionPending(void) {
    return native_execution.state()==NativeDebug::State::Pending;
}
bool DEBUG_NativeExecutionInFlight(void) {
    return native_execution.state()==NativeDebug::State::Pending ||
           native_execution.state()==NativeDebug::State::Active;
}
NativeDebug::Context DEBUG_NativeExecutionContext(void) {
    return native_execution.context(DOSBOX_MachineServiceOwner().token);
}
bool DEBUG_AdmitNativeObservation(NativeDebug::Attachment attachment, NativeDebug::Context expected) {
    if (!native_execution.admits(attachment) || DEBUG_NativeExecutionInFlight() ||
        !debugging || debug_running || DEBUG_ContinuationUnavailable() ||
        DOSBOX_MachineContextAdmission()!=MachineService::Denial::None ||
        !NativeControl::rawEffectAllowed()) return false;
    const auto current=DEBUG_NativeExecutionContext();
#if C_DEBUG_LUA
    // Accepted checkpoint installer requires disposal of the pre-load frame.
    // Do not borrow its still-held debugger notification after restore/poison.
    if (Debugger_LuaCheckpointOwnerMustDispose(current.invocation)) return false;
#endif
    return expected.invocation && expected.checkpoint &&
        expected.invocation==current.invocation && expected.checkpoint==current.checkpoint;
}
bool DEBUG_AdmitNativeState(NativeDebug::Attachment attachment, NativeDebug::Context expected,
                           const NativeControl::Lease *held, bool require_stopped) {
    if (!native_execution.admits(attachment) || DEBUG_NativeExecutionInFlight() ||
        DEBUG_ContinuationUnavailable() ||
        DOSBOX_MachineContextAdmission()!=MachineService::Denial::None ||
        (held ? !NativeControl::owner().matches(*held) : NativeControl::owner().busy())) return false;
#if C_DEBUG_LUA
    // Live evaluators temporarily project a stopped debugger; that projection
    // is not admission. Use the saved/deliberately selected native state.
    if (require_stopped && !Debugger_LiveEffectiveStopped()) return false;
#else
    if (require_stopped && (!debugging || debug_running)) return false;
#endif
    const auto current=DEBUG_NativeExecutionContext();
    return expected.invocation && expected.checkpoint &&
        expected.invocation==current.invocation && expected.checkpoint==current.checkpoint;
}
bool DEBUG_AdvanceNativeStateContext(NativeDebug::Attachment attachment, NativeDebug::Context expected,
                                    NativeControl::Lease held) {
    return DEBUG_AdmitNativeState(attachment,expected,&held) &&
        native_execution.checkpointChanged(expected);
}
bool DEBUG_NativeStateFile(NativeDebug::Attachment attachment, NativeDebug::Context expected,
                           const std::string &path, bool load, StateArchive::Effects *effects,
                           const StateArchive::Budget *budget) {
    if (effects) *effects = {};
    if (path.empty() || path.find('\0') != std::string::npos ||
        !DEBUG_AdmitNativeState(attachment, expected)) return false;
    const auto current = DOSBOX_MachineServiceOwner();
    if (NativeControl::owner().acquire({expected.invocation, expected.checkpoint}, current,
            DOSBOX_MachineServiceRelation(expected.invocation), expected.checkpoint, true) !=
            NativeControl::Admission::ok) return false;
    const auto lease = NativeControl::owner().lease();
    struct Release {
        NativeControl::Lease lease;
        ~Release() { NativeControl::owner().release(lease); }
    } release{lease};
    const bool success = load ? SaveState::instance().loadFile(path) :
        (effects ? SaveState::instance().saveFile(path, *effects, budget) : SaveState::instance().saveFile(path));
    if (success && load && !DEBUG_AdvanceNativeStateContext(attachment, expected, lease))
        E_Exit("Restored state cannot retire prior native context");
    return success;
}
const NativeDebug::Result *DEBUG_NativeExecutionResult(NativeDebug::Attachment attachment) {
    return native_execution.result(attachment);
}
static bool DEBUG_HoldNativeExecution(void) {
    if (!native_execution.activeRequest()) return false;
    // Stop publication cannot toggle execution, require a tty or apply the
    // interactive run-on-entry policy. Final notification waits for unwind.
    CPU_CycleLeft+=CPU_Cycles;
    CPU_Cycles=0;
    DEBUG_EnterCommonStoppedState();
    CBreakpoint::DeactivateBreakpoints();
    exitLoop=true;
    Bitu DEBUG_Loop(void);
    DOSBOX_SetLoop(DEBUG_Loop);
    return true;
}
static uint64_t DEBUG_ExecutionWallMilliseconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool DEBUG_NativeExecutionBudgetExpired(void) {
    const auto *request=native_execution.activeRequest();
    if (!request || request->expected.invocation!=DOSBOX_MachineServiceOwner().token)
        return false;
    if (!native_execution.budgetExpired(DEBUG_ExecutionWallMilliseconds(),PIC_FullIndex()))
        return false;
    DEBUG_HoldNativeExecution();
    return true;
}
bool DEBUG_DetachNativeExecution(NativeDebug::Attachment attachment) {
    if (!native_execution.close(attachment)) return false;
    const auto *active=native_execution.activeRequest();
    if (active && active->attachment.serial==attachment.serial) {
        // Departure revokes only this attachment's continuing grant; defer
        // settlement and retirement until the decoder/callback owner unwinds.
        native_execution.detach(attachment);
        DEBUG_HoldNativeExecution();
        return false;
    }
    // A live evaluator/state lease is not disposable instrumentation.
    if (NativeControl::owner().busy()) return false;
#if C_DEBUG_LUA
    if (Debugger_PredicateHeld() || !Debugger_LuaSessionPrepareDetach()) return false;
#endif
    if (const auto *pending=native_execution.pendingRequest()) {
        const auto selected=*pending;
        DEBUG_CleanupNativeExecution(selected);
        native_execution.settle(0,true);
        DEBUG_ProjectNativeTerminal(attachment);
    }
    CBreakpoint::DeleteSession(attachment.serial);
    // CPU syscall and VGA retrace stops are session instrumentation, not
    // CBreakpoint entries. Active work and live leases have drained here.
    Clear_SYSENTER_Debug();
    DEBUG_HaltOnRetrace=false;
    // Parser-owned observations and command input are session resources too.
    // Keep guest edits, but do not expose an old search/symbol or execute an
    // old partially typed command after a different debugger attaches.
    delete MEMFINDInstance;
    MEMFINDInstance=nullptr;
    CDebugVar::DeleteAll();
    histBuff.clear();
    histBuffPos=histBuff.end();
    ClearInputLine();
#if C_HEAVY_DEBUG
    DEBUG_CleanupSessionInstrumentation();
#endif
    if (instrumentation_attachment.serial==attachment.serial) instrumentation_attachment={};
    return native_execution.detach(attachment);
}
bool DEBUG_ReleaseNativeExecutionResult(NativeDebug::Attachment attachment, uint64_t operation) {
    const auto *result=native_execution.result(attachment);
    if (!result || result->operation!=operation) return false;
    return native_execution.release(attachment,operation);
}
NativeDebug::Attachment DEBUG_AttachNativeExecution(NativeDebug::Frontend frontend, uint64_t principal) {
    return native_execution.attach(frontend,principal);
}
bool DEBUG_AdmitNativeEffect(NativeDebug::Attachment attachment) {
    const auto context=DOSBOX_MachineContextAdmission();
    // Standalone stopped Lua diagnostics predate the machine invocation. They
    // may use synchronous stopped effects, but this grants no invocation or
    // checkpoint: queued execution still validates both at enqueue/activation.
    const bool admitted=native_execution.admits(attachment) && native_execution.canEnqueue() &&
        debugging && !debug_running &&
        (context==MachineService::Denial::None || context==MachineService::Denial::NoOwner) &&
        NativeControl::diagnosticEffectAllowed() && !DEBUG_ContinuationUnavailable();
    if (admitted) instrumentation_attachment=attachment;
    return admitted;
}
static NativeDebug::Attachment DEBUG_CommandSession() {
    return command_attachment;
}
NativeDebug::Admission DEBUG_RequestNativeExecution(NativeDebug::Request request) {
    if (NativeDebug::validateConditions(request.stop_any)) return NativeDebug::Admission::Capacity;
    if (!request.stop_any.empty()) {
        if (!request.resume || cpudecoder!=CPU_Core_Normal_Run) return NativeDebug::Admission::UnsafeContext;
        for (const auto &condition:request.stop_any) {
            if (condition.kind==NativeDebug::StopCondition::Kind::Predicate &&
                request.frontend!=NativeDebug::Frontend::Lua)
                return NativeDebug::Admission::UnsafeContext; // worker-local rendezvous only
            NativeDebug::TextSnapshot snapshot;
            if (condition.kind==NativeDebug::StopCondition::Kind::Text &&
                !VGA_CopyTextSnapshot(condition.region,snapshot)) return NativeDebug::Admission::UnsafeContext;
        }
    }
    const auto context=DEBUG_NativeExecutionContext();
    if (native_execution.state()!=NativeDebug::State::Empty)
        return native_execution.state()==NativeDebug::State::Terminal?
            NativeDebug::Admission::Capacity:NativeDebug::Admission::Busy;
    const bool safe=debugging && !debug_running &&
        DOSBOX_MachineContextAdmission()==MachineService::Denial::None &&
        NativeControl::diagnosticEffectAllowed() && !DEBUG_ContinuationUnavailable();
    if (!safe) return NativeDebug::Admission::UnsafeContext;
    if (request.action==NativeDebug::Request::Action::Phase &&
        (!request.phase_limit || request.phase_limit>10000 ||
         !request.phase_wall_ms || request.phase_wall_ms>100 || !request.retain_result))
        return NativeDebug::Admission::Capacity;
    if (!context.invocation || !context.checkpoint ||
        context.invocation!=request.expected.invocation ||
        context.checkpoint!=request.expected.checkpoint)
        return NativeDebug::Admission::StaleContext;
    if (!native_execution.admits(request.attachment))
        return NativeDebug::Admission::UnsafeContext;
    instrumentation_attachment=request.attachment;
    return native_execution.enqueue(request,context,safe);
}


bool DEBUG_ResumeNativeExecution(NativeDebug::Frontend frontend, bool watch, bool retrace, NativeDebug::Attachment attachment,
                                 const std::vector<NativeDebug::StopCondition> &conditions) {
    NativeDebug::Request request;
    request.attachment=attachment;
    request.frontend=frontend;
    request.expected=DEBUG_NativeExecutionContext();
    request.resume=true;request.watch=watch;
    request.retrace=retrace;request.normal_resume=true;
    request.stop_any=conditions;
    if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted) {
        DEBUG_ShowMsg("RUN admission rejected: unsupported or unavailable stopped context\n");return false;
    }
    return true;
}

// Finish only the operation currently owned by this caller. This action never
// invokes a decoder or callback; no request copy survives across guest work in
// this completion primitive. Native return seams can supply the actual current
// invocation rather than implicitly reviving the request's start context.
static void DEBUG_FinishNativeExecution(uint64_t operation, Bits result) {
    if (!native_execution.ownsActive(operation)) return;
    const auto request=*native_execution.activeRequest();
    DEBUG_CleanupNativeExecution(request);
    // Predicate's internal suppression grant is not a user-visible hold.
    // All other completed continuing grants must restore hold before publish.
    if (!result && (!request.resume ||
                    request.frontend!=NativeDebug::Frontend::Predicate)) {
        DEBUG_EnterCommonStoppedState();
        CBreakpoint::DeactivateBreakpoints();
        SetCodeWinStart();
        DEBUG_DrawScreen();
    }
    const bool target_exit=request.frontend==NativeDebug::Frontend::Agent &&
        agent_target_exited && request.expected.invocation==agent_target_owner;
    native_execution.settle(native_execution.decoderCalls(),result!=0 && !target_exit,
        SegValue(cs),reg_eip,reg_ax,reg_bx,reg_cx,reg_dx,
        target_exit?NativeDebug::Terminal::ProgramExit:NativeDebug::Terminal::Held,
        DOSBOX_MachineServiceOwner().token);
#if defined(C_DOSBOX_AGENT)
    if (request.frontend==NativeDebug::Frontend::Agent)
        DEBUG_ProjectNativeTerminal(request.attachment);
    else if (!result)
        dosbox_agent::AGENT_NotifyDebuggerStopped(SegValue(cs),reg_eip);
#endif
    if (request.frontend==NativeDebug::Frontend::Interactive && !request.stop_any.empty()) {
        const auto &stop=native_execution.result(request.attachment)->stop;
        DEBUG_ShowMsg("RUN stop: %s index=%u error=%s ticks=%llu..%llu host_ms=%llu advanced=%u context=%04X:%08X\n",
            stop.reason.c_str(),stop.condition_index,stop.error.c_str(),
            (unsigned long long)stop.start_tick,(unsigned long long)stop.end_tick,
            (unsigned long long)stop.host_elapsed_ms,unsigned(stop.guest_advanced),SegValue(cs),reg_eip);
        if (!stop.text.cells.empty()) {
            const auto &text=stop.text;
            DEBUG_ShowMsg("RUN text: tick=%llu start=%u stride=%u codepage=%u region=%u,%u,%u,%u match=%u,%u cells=hex-char-attribute\n",
                (unsigned long long)text.tick,text.start,text.stride,unsigned(text.dos_codepage),
                text.region.x,text.region.y,text.region.width,text.region.height,stop.match_x,stop.match_y);
            static const char hex[]="0123456789ABCDEF";
            for (unsigned row=0;row<text.region.height;++row) {
                std::string cells;
                for (unsigned col=0;col<text.region.width*2;++col) {
                    const auto b=text.cells[row*text.region.width*2+col];
                    cells+=hex[b>>4];cells+=hex[b&15];
                }
                DEBUG_ShowMsg("RUN text row %u: %s\n",text.region.y+row,cells.c_str());
            }
        }
    }
    // Cleanup and immutable publication precede exact slot release. Selected
    // synchronous adapters retain their existing result ownership policy.
    if (request.frontend!=NativeDebug::Frontend::Bridge && !request.retain_result) {
        const auto *terminal=native_execution.result(request.attachment);
        native_execution.release(request.attachment,terminal->operation);
    }
}

// Built-ins use the same slot/evaluator as the worker, without a worker rendezvous.
static bool DEBUG_EvaluateNativeConditions() {
    const NativeDebug::StopObservations observations{
        [](const NativeDebug::StopCondition &c) {
            const auto *request=native_execution.activeRequest();
            return request && (native_condition_address_hit || native_execution.decoderCalls()==0) &&
                DOSBOX_MachineServiceOwner().token==request->expected.invocation &&
                SegValue(cs)==c.cs && reg_eip==c.ip;
        }, VGA_CopyTextSnapshot,
        [](bool &error) {error=true;return false;}
    };
    return NativeDebug::evaluateConditions(native_execution.conditions(),native_execution.stopRuntime(),PIC_Ticks,
        PIC_Ticks!=native_execution.stopRuntime().evidence.start_tick || native_execution.decoderCalls()!=0,observations);
}
static bool DEBUG_ObserveBuiltinAddress(uint64_t id) {
    const auto *request=native_execution.activeRequest();
    if (!request || request->frontend==NativeDebug::Frontend::Lua ||
        std::find(native_condition_breakpoints.begin(),native_condition_breakpoints.end(),id)==native_condition_breakpoints.end()) return false;
    if (CBreakpoint::HasIndependentAt(id,GetAddress(SegValue(cs),reg_eip))) return false;
    native_condition_address_hit=true;
    const bool matched=DEBUG_EvaluateNativeConditions();
    native_condition_address_hit=false;
    return matched;
}
static void DEBUG_NativeConditionFrontier(void *,const NativeFrontier::Descriptor &seam) {
    const auto *request=native_execution.activeRequest();
    if (!request || request->frontend==NativeDebug::Frontend::Lua || request->stop_any.empty() ||
        !request->resume || exitLoop || debugging || debug_running) return;
    if (seam.kind==NativeFrontier::Kind::ExitFence || seam.kind==NativeFrontier::Kind::LoopStop) return;
    if (DEBUG_EvaluateNativeConditions()) DEBUG_HoldNativeExecution();
}

bool DEBUG_DeferPredicateInstruction(void) {
    NativeDebug::Request request;
    request.frontend=NativeDebug::Frontend::Predicate;
    request.attachment=DEBUG_AttachNativeExecution(request.frontend,0);
    request.expected=DEBUG_NativeExecutionContext();
    request.resume=true;
    return DEBUG_RequestNativeExecution(request)==NativeDebug::Admission::Accepted;
}

#if C_DEBUG_LUA
static bool local_driver_preparing=false;
static bool local_driver_host_break=false;
static LuaDriver::Handoff local_driver_handoff;
static LuaDriver::WorkerResult local_driver_result;
static std::thread local_driver_thread;
static NativeDebug::Attachment local_driver_attachment;
static bool local_driver_started=false;
// Selected natural EXEC departure is not target absence: retain the request
// through native return restoration and the shell's actual cleanup fence.
static uint64_t local_driver_selected_owner=0;
static bool local_driver_selected_departed=false;
static void LocalDriverOwnerDeparting(uint64_t token) {
    if(local_driver_started && token==local_driver_selected_owner)
        local_driver_selected_departed=true;
}
static bool local_driver_fatal=false;
static std::thread::id local_driver_native_thread;
// Retained poisoned/failed-cleanup Jobs fence this attachment, not just saves.
static bool local_driver_unusable=false;
static uint64_t local_driver_operation=0;
static bool local_driver_continuous=false,local_driver_target=false;
static bool local_driver_legacy_until=false;
static uint16_t local_driver_target_cs=0;
static uint32_t local_driver_target_ip=0;
static uint64_t local_driver_target_owner=0;
static bool local_driver_hit=false,local_driver_observer_error=false;
static bool local_driver_address_hit=false;
static bool LocalDriverConditions();
struct LocalDriverRegistration {uint64_t lua_id,native_id;bool observer;};
static std::vector<LocalDriverRegistration> local_driver_registrations;
static LuaDriver::Response LocalDriverImage(const NativeFrontier::Descriptor &seam) {
    LuaDriver::Response image;
    image.context=DEBUG_NativeExecutionContext();
    const auto r=Debugger_GetRegisters();
    image.cs=r.cs;image.ip=r.eip;image.ax=r.eax;image.bx=r.ebx;image.cx=r.ecx;image.dx=r.edx;
    image.eax=r.eax;image.ebx=r.ebx;image.ecx=r.ecx;image.edx=r.edx;
    image.esi=r.esi;image.edi=r.edi;image.ebp=r.ebp;image.esp=r.esp;
    // Heavy normal-core hits precede the decoder's FillFlags(). Evaluate the
    // copied image without changing the native deferred arithmetic state.
    image.flags=CPU_InspectFlags();
    image.ds=r.ds;image.es=r.es;image.fs=r.fs;image.gs=r.gs;image.ss=r.ss;
    image.code_big=r.code_big;
    image.registers_valid=seam.kind!=NativeFrontier::Kind::ExitFence;
    image.continuation=image.registers_valid && image.context.invocation && image.context.checkpoint;
    image.partial=seam.decoder_entered || seam.callback_entered ||
                  seam.kind==NativeFrontier::Kind::PartialParent;
    switch(seam.kind) {
    case NativeFrontier::Kind::Decoder:image.boundary="decoder";break;
    case NativeFrontier::Kind::CallbackEntry:image.boundary="callback-entry";break;
    case NativeFrontier::Kind::Predispatch:image.boundary="predispatch";break;
    case NativeFrontier::Kind::PostTick:image.boundary="post-tick";break;
    case NativeFrontier::Kind::NestedEntry:image.boundary="nested-entry";break;
    case NativeFrontier::Kind::NativeReturn:image.boundary="native-return";break;
    case NativeFrontier::Kind::PartialParent:image.boundary="partial-parent";break;
    case NativeFrontier::Kind::LoopStop:image.boundary="loop-stop";break;
    case NativeFrontier::Kind::ExitFence:image.boundary="exit-fence";break;
    }
    return image;
}
static bool LocalDriverObserveHit(uint64_t id) {
    const auto *active=native_execution.activeRequest();
    if(!active || active->frontend!=NativeDebug::Frontend::Lua)return true;
    auto found=std::find_if(local_driver_registrations.begin(),local_driver_registrations.end(),
        [id](const LocalDriverRegistration &r){return r.native_id==id;});
    if(id && found==local_driver_registrations.end()) {
        auto target=std::find(native_condition_breakpoints.begin(),native_condition_breakpoints.end(),id);
        if(target!=native_condition_breakpoints.end()) {
            // Independent active registrations at this address win; their
            // observers are still serviced by the ordinary hit producer.
            if(CBreakpoint::HasIndependentAt(id,GetAddress(SegValue(cs),reg_eip)))return true;
            local_driver_address_hit=true;
            LocalDriverConditions();
            local_driver_address_hit=false;
        }
        return true;
    }
    if(id && !found->observer){local_driver_hit=true;return true;}
    static uint64_t next_event=0;
    if(next_event==UINT64_MAX)return true;
    // This stack frame is the authority. No ordinary admission or general
    // Active permission is granted, and it expires before this hit returns.
    const auto context=native_execution.eventContext(local_driver_attachment,
        native_execution.operation(),DOSBOX_MachineServiceOwner().token);
    if(!context.invocation || DOSBOX_MachineFrontierAdmission()!=MachineService::Denial::None ||
       NativeControl::owner().busy()) {
        local_driver_observer_error=true;local_driver_hit=true;return true;
    }
    NativeFrontier::Descriptor seam{};seam.kind=NativeFrontier::Kind::Decoder;
    LuaDriver::Event event;event.ordinal=++next_event;event.registration=id?found->lua_id:0;
    event.image=LocalDriverImage(seam);event.image.context=context;
    event.image.start_tick=native_execution.stopRuntime().evidence.start_tick;
    event.image.end_tick=PIC_Ticks;
    event.image.host_elapsed_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-native_execution.stopRuntime().host_start).count();
    // Provisional event identity authorizes reads only, never continuation.
    event.image.continuation=false;
    if(!local_driver_handoff.offerEvent(std::move(event))) {
        local_driver_observer_error=true;local_driver_hit=true;return true;
    }
    for(;;) {
        LuaDriver::Request read;
        if(local_driver_handoff.takeObserverRead(read)) {
            LuaDriver::Response response;
            const bool valid=read.context.invocation==context.invocation &&
                read.context.checkpoint==context.checkpoint;
            if(!valid)response.error="expired observer context";
            else if(read.operation==LuaDriver::Operation::TextSnapshot) {
                if(!VGA_CopyTextSnapshot(read.region,response.text))response.error="unsupported_text_layout";
            } else {
                response.bytes.resize(read.length);
                if(!PAGING_InspectLinear(read.address,read.length,response.bytes.data()))
                    response.error="unsupported side-effect-free read";
            }
            const bool violation=!response.error.empty();
            local_driver_handoff.completeObserverRead(std::move(response),violation);
        }
        LuaDriver::Verdict verdict;
        if(local_driver_handoff.takeVerdict(verdict)) {
            local_driver_observer_error=verdict.violation || verdict.error;
            local_driver_hit=verdict.propagate || local_driver_observer_error;
            // Consume queued host events before conditions after a held verdict.
            // False must not turn a requested host break/close into success.
            GFX_Events();
            if(local_driver_host_break || local_driver_handoff.closing())return true;
            if(!local_driver_hit && id) {
                // A suppressed observer is not a suppressed requested address.
                // Evaluate the ordered OR at this actual producer only after
                // the independent verdict has settled.
                local_driver_address_hit=true;
                const bool matched=LocalDriverConditions();
                local_driver_address_hit=false;
                if(matched)return true;
            }
            return local_driver_hit || exitLoop || local_driver_handoff.closing();
        }
        GFX_Events();SDL_Delay(1);
    }
}
static bool LocalDriverConditions() {
    const NativeDebug::StopObservations observations{
        [](const NativeDebug::StopCondition &c) {
            return (local_driver_address_hit || native_execution.decoderCalls()==0) &&
                DOSBOX_MachineServiceOwner().token==local_driver_target_owner && SegValue(cs)==c.cs && reg_eip==c.ip;
        },
        VGA_CopyTextSnapshot,
        [](bool &error) {
            const bool match=LocalDriverObserveHit(0);
            error=local_driver_observer_error;local_driver_hit=false;return match;
        }
    };
    const bool stopped=NativeDebug::evaluateConditions(native_execution.conditions(),
        native_execution.stopRuntime(),PIC_Ticks,
        PIC_Ticks!=native_execution.stopRuntime().evidence.start_tick ||
            native_execution.decoderCalls()!=0,observations);
    if(local_driver_host_break && !local_driver_observer_error) {
        // Host break is terminal arbitration, not a condition match.
        native_execution.stopRuntime().evidence={};
        return true;
    }
    return stopped;
}
bool Debugger_LocalDriverClosing() {return local_driver_handoff.closing();}
static void LocalDriverRetire() {
    // luaClosed is published after lua_close; never join an unpublished waiter.
    local_driver_thread.join();
    for(const auto &registration:local_driver_registrations) {
        bool cleared=false;
        // Disposal revokes our exact retained resources even after the machine
        // invocation has unwound. It is not admission for a new guest effect.
        if(!CBreakpoint::DeleteById(registration.native_id,DebuggerBreakpointOwner::Lua,
                                   &cleared) || !cleared)
            E_Exit("Lua driver registration cleanup failed");
    }
    local_driver_registrations.clear();
    if(!DEBUG_DetachNativeExecution(local_driver_attachment))
        E_Exit("Lua driver detach failed");
    LOG_MSG("Lua driver closed: %s",local_driver_result.completed?"completed":local_driver_result.error.c_str());
}
static void LocalDriverFrontier(void *,const NativeFrontier::Descriptor &seam) {
    if(seam.kind==NativeFrontier::Kind::Predispatch || seam.kind==NativeFrontier::Kind::PostTick) {
        const auto *active=native_execution.activeRequest();
        if((!active || !active->resume || active->frontend!=NativeDebug::Frontend::Lua ||
           native_execution.conditions().empty()) &&
           !(local_driver_operation && native_execution.result(local_driver_attachment)))return;
    }
    // Loaded disposal is earlier and wins. For an ordinary selected return do
    // not expose the restored shell as a new guest context or settle a step.
    if(local_driver_selected_departed && seam.kind!=NativeFrontier::Kind::ExitFence)return;
    if(seam.kind==NativeFrontier::Kind::ExitFence && local_driver_started) {
        if(local_driver_operation) {
            DEBUG_CleanupNativeExecution(*native_execution.activeRequest());
            native_execution.settle(native_execution.decoderCalls(),false,0,0,0,0,0,0,
                                    NativeDebug::Terminal::ProgramExit);
            const auto *terminal=native_execution.result(local_driver_attachment);
            if(!terminal || terminal->operation!=local_driver_operation)
                E_Exit("Lua driver exit terminal identity lost");
            LuaDriver::Response response;
            response.reason=local_driver_selected_departed &&
                !Debugger_LuaCheckpointOwnerMustDispose(seam.invocation)?
                "target_exit":"guest_lifetime_ended";
            response.boundary="exit-fence";
            if(!native_execution.release(local_driver_attachment,local_driver_operation))
                E_Exit("Lua driver exit release failed");
            local_driver_operation=0;
            if(!local_driver_handoff.complete(std::move(response)))
                E_Exit("Lua driver exit response lost");
        }
        local_driver_handoff.close();
        while(!local_driver_handoff.isLuaClosed()){GFX_Events();SDL_Delay(1);}
        LocalDriverRetire();
        return;
    }
#if !C_HEAVY_DEBUG
    if(local_driver_suppressed_breakpoint) {
        local_driver_suppressed_breakpoint=0;
        CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
    }
#endif
    if (!NativeFrontier::frontiers().ownsHold(seam) ||
        DOSBOX_MachineFrontierAdmission()!=MachineService::Denial::None ||
        NativeControl::owner().busy()) return;
    if (!local_driver_started) {
        const auto owner=DOSBOX_MachineServiceOwner();
        if(!local_driver_attachment.serial) {
            if(owner.kind!=MachineService::Kind::RootGuest || owner.depth!=1 ||
               seam.decoder_entered || seam.callback_entered)return;
            local_driver_attachment=native_execution.attach(NativeDebug::Frontend::Lua,0);
            if(!local_driver_attachment.serial)E_Exit("Lua driver attachment unavailable");
            if(control->lua_driver.program_set) {
                if(!Debugger_LocalDriverArm(local_driver_attachment,
                        DEBUG_NativeExecutionContext(),control->lua_driver.program))
                    E_Exit("Lua driver startup program arm refused");
                return; // natural bootstrap, never launch from held service
            }
        }
        if(control->lua_driver.program_set) {
            const int entry=Debugger_LocalDriverEntry();
            if(entry<0)E_Exit("Lua driver startup program nonarrival");
            if(!entry)return;
            local_driver_selected_owner=seam.invocation;
            if(control->lua_driver.archive_set) {
                if(const char *error=Debugger_LocalDriverLoad(local_driver_attachment,
                        DEBUG_NativeExecutionContext(),control->lua_driver.archive))
                    E_Exit("Lua driver startup archive: %s",error);
            }
        }
        DEBUG_EnterCommonStoppedState();
        local_driver_thread=std::thread(LuaDriver::runWorker,std::cref(control->lua_driver),
                                       std::ref(local_driver_handoff),std::ref(local_driver_result));
        local_driver_started=true;
        local_driver_handoff.ready();
    } else {
        const auto *active=native_execution.activeRequest();
        if(active && active->attachment.serial==local_driver_attachment.serial) {
            // RUN ignores synthetic step frontiers. A genuine stopped loop,
            // not decoder/callback entry, is its completion boundary.
            if(active->resume) {
                if(seam.kind!=NativeFrontier::Kind::LoopStop || !debugging || debug_running) {
                    if(local_driver_handoff.closing() || exitLoop || local_driver_hit || local_driver_observer_error)return;
                    if(!LocalDriverConditions())return;
                    DEBUG_EnterCommonStoppedState();
                }
            } else if(seam.kind==NativeFrontier::Kind::LoopStop)return;
        }
        if(active && active->attachment.serial==local_driver_attachment.serial) {
            DEBUG_FinishNativeExecution(native_execution.operation(),0);
        }
        if(local_driver_operation && native_execution.activeRequest())return;
        if(local_driver_operation) {
            const auto *terminal=native_execution.result(local_driver_attachment);
            if(!terminal || terminal->operation!=local_driver_operation)
                E_Exit("Lua driver terminal identity lost");
            auto image=LocalDriverImage(seam);
            image.reason=local_driver_continuous?"manual":"step";
            if(local_driver_target && seam.invocation==local_driver_target_owner &&
               SegValue(cs)==local_driver_target_cs &&
               reg_eip==local_driver_target_ip)image.reason="target";
            const auto &local_driver_condition_result=terminal->stop;
            if(local_driver_condition_result.condition_index || !local_driver_condition_result.error.empty()) {
                image.reason=local_driver_legacy_until?"target":"condition";image.error=local_driver_condition_result.error;
                image.condition_index=local_driver_condition_result.condition_index;
                image.start_tick=local_driver_condition_result.start_tick;
                image.end_tick=local_driver_condition_result.end_tick;
                image.host_elapsed_ms=local_driver_condition_result.host_elapsed_ms;
                image.guest_advanced=local_driver_condition_result.guest_advanced;
                image.text=local_driver_condition_result.text;
                image.match_x=local_driver_condition_result.match_x;
                image.match_y=local_driver_condition_result.match_y;
            }
            if(local_driver_hit)image.reason="breakpoint";
            if(local_driver_observer_error)image.reason="error";
            if(terminal->failed)image.error="native execution failed";
            if(!native_execution.release(local_driver_attachment,local_driver_operation))
                E_Exit("Lua driver result release failed");
            local_driver_operation=0;
            local_driver_handoff.complete(std::move(image));
        }
    }
    for(;;) {
        if(local_driver_handoff.isLuaClosed()) {
            LocalDriverRetire();
            throw 1;
        }
        LuaDriver::Request request;
        if(local_driver_handoff.take(request)) {
            auto response=LocalDriverImage(seam);
            const auto current=response.context;
            if(local_driver_unusable)response.error="native state cleanup failed; driver unusable";
            else if(request.operation==LuaDriver::Operation::Read &&
               (!request.length || request.length>4096)) response.error="unsupported read length";
            else if((request.operation==LuaDriver::Operation::Registers ||
                     request.operation==LuaDriver::Operation::Read ||
                     request.operation==LuaDriver::Operation::TextSnapshot ||
                     request.operation==LuaDriver::Operation::RunUntil) &&
               (request.context.invocation!=current.invocation ||
                request.context.checkpoint!=current.checkpoint)) response.error="stale context";
            else if((request.operation==LuaDriver::Operation::Breakpoint ||
                     request.operation==LuaDriver::Operation::Watchpoint ||
                     request.operation==LuaDriver::Operation::Remove) &&
                    // The shell EXEC's outer callback remains entered for the
                    // whole natural program. Its current ShellExec decoder is
                    // ordinary; boundary kind and entered decoder exclude the
                    // restricted wrapper/partial seams, not callback ancestry.
                    ((seam.kind!=NativeFrontier::Kind::LoopStop &&
                      seam.kind!=NativeFrontier::Kind::Decoder) ||
                     seam.decoder_entered ||
                     (DOSBOX_MachineServiceOwner().kind!=MachineService::Kind::RootGuest &&
                      DOSBOX_MachineServiceOwner().kind!=MachineService::Kind::ShellExecGuest)))
                response.error="instrumentation requires ordinary stopped context";
            else if(request.operation==LuaDriver::Operation::TextSnapshot) {
                if(!VGA_CopyTextSnapshot(request.region,response.text))response.error="unsupported_text_layout";
            } else if(request.operation==LuaDriver::Operation::Read) {
                response.bytes.resize(request.length);
                if(!PAGING_InspectLinear(request.address,request.length,response.bytes.data()))
                    response.error="unsupported side-effect-free read";
            } else if(request.operation==LuaDriver::Operation::StateReady ||
                      request.operation==LuaDriver::Operation::Save) {
                if(!Debugger_LiveOwnsStop(current.invocation) &&
                   !Debugger_LocalClaimStop(local_driver_attachment,current,seam))
                    response.error="state operation requires ordinary owned stop";
                else if(request.operation==LuaDriver::Operation::StateReady)
                    Debugger_LocalDriverStateReady(local_driver_attachment,current,response);
                else {
                    Debugger_LocalDriverSave(local_driver_attachment,current,request.path,
                                             request.timeout_ms,response.save);
                    if(!response.save.usable || response.save.poisoned || response.save.cleanup_failed ||
                       (!response.save.retired && response.save.code=="saved"))
                        local_driver_unusable=true;
                    if(!response.save.completed)response.error=response.save.code;
                }
            } else if(request.operation==LuaDriver::Operation::Breakpoint) {
                if(cpudecoder!=CPU_Core_Normal_Run) {
                    response.error="unsupported-core";
                    local_driver_handoff.complete(std::move(response));continue;
                }
                const auto address=Debugger_ResolveAddress(request.cs,request.ip);
                DebuggerBreakpointCreateOptions options{};
                options.enabled=true;options.owner=DebuggerBreakpointOwner::Lua;
                uint64_t id=0;
                if(Debugger_BreakpointSet(address,options,&id,local_driver_attachment)!=
                   DebuggerBreakpointFailure::None)response.error="breakpoint refused";
                else {
                    local_driver_registrations.push_back({request.registration,id,request.observer});
                    response.registration=request.registration;
                }
            } else if(request.operation==LuaDriver::Operation::Watchpoint) {
                uint64_t id=0;
#if defined(C_DOSBOX_AGENT)
                if(cpudecoder==CPU_Core_Normal_Run &&
                   CreateMemoryBreakpoint(request.cs,request.address,false,false,&id,
                       local_driver_attachment,current,DebuggerBreakpointOwner::Lua)) {
                    local_driver_registrations.push_back({request.registration,id,request.observer});
                    response.registration=request.registration;
                } else
#endif
                    response.error="unsupported watchpoint";
            } else if(request.operation==LuaDriver::Operation::Remove) {
                auto found=std::find_if(local_driver_registrations.begin(),local_driver_registrations.end(),
                    [&](const LocalDriverRegistration &r){return r.lua_id==request.registration;});
                bool cleared=false;
                if(found==local_driver_registrations.end() ||
                   Debugger_BreakpointClear(found->native_id,DebuggerBreakpointOwner::Lua,
                       &cleared,local_driver_attachment)!=DebuggerBreakpointFailure::None)
                    response.error="registration removal refused";
                else local_driver_registrations.erase(found);
            } else if(request.operation==LuaDriver::Operation::Step ||
                      request.operation==LuaDriver::Operation::Run ||
                      request.operation==LuaDriver::Operation::Over ||
                      request.operation==LuaDriver::Operation::RunUntil) {
                if(request.operation==LuaDriver::Operation::Over &&
                   ((seam.kind!=NativeFrontier::Kind::LoopStop &&
                     seam.kind!=NativeFrontier::Kind::Decoder) ||
                    seam.decoder_entered ||
                    (DOSBOX_MachineServiceOwner().kind!=MachineService::Kind::RootGuest &&
                     DOSBOX_MachineServiceOwner().kind!=MachineService::Kind::ShellExecGuest))) {
                    response.error="unsupported_context";
                    local_driver_handoff.complete(std::move(response));
                    continue;
                }
                NativeDebug::Request execution;
                execution.frontend=NativeDebug::Frontend::Lua;
                execution.attachment=local_driver_attachment;execution.expected=current;
                execution.completion=NativeDebug::Request::Completion::NativeBoundary;
                execution.resume=request.operation==LuaDriver::Operation::Run ||
                                 request.operation==LuaDriver::Operation::RunUntil;
                execution.over=request.operation==LuaDriver::Operation::Over;
                if(request.operation==LuaDriver::Operation::RunUntil &&
                   cpudecoder!=CPU_Core_Normal_Run) {
                    response.error="unsupported-core";
                    local_driver_handoff.complete(std::move(response));
                    continue;
                }
                execution.stop_any=request.stop_any;
                if(request.operation==LuaDriver::Operation::RunUntil) {
                    NativeDebug::StopCondition address;
                    address.cs=request.cs;address.ip=request.ip;
                    execution.stop_any.push_back(address);
                }
                if(const char *error=NativeDebug::validateConditions(execution.stop_any))response.error=error;
                if(!execution.stop_any.empty() && cpudecoder!=CPU_Core_Normal_Run)response.error="unsupported-core";
                for(const auto &condition:execution.stop_any)
                    if(condition.kind==NativeDebug::StopCondition::Kind::Text &&
                       !VGA_CopyTextSnapshot(condition.region,response.text))response.error="unsupported_text_layout";
                if(!response.error.empty()) {local_driver_handoff.complete(std::move(response));continue;}
                execution.retain_result=true;
                if(native_execution.enqueue(execution,current,true)!=NativeDebug::Admission::Accepted)
                    response.error="native execution refused";
                else {
                    local_driver_operation=native_execution.operation();
                    local_driver_hit=false;local_driver_observer_error=false;
                    local_driver_host_break=false;
                    local_driver_continuous=execution.resume;
                    local_driver_target=false; // RunUntil uses the address condition owner.
                    local_driver_legacy_until=request.operation==LuaDriver::Operation::RunUntil;
                    local_driver_target_cs=request.cs;local_driver_target_ip=request.ip;
                    local_driver_target_owner=current.invocation;
                    native_execution.stopRuntime().begin(execution.stop_any.size(),PIC_Ticks);
                    local_driver_preparing=true;
                    DEBUG_DispatchNativeExecution();
                    local_driver_preparing=false;
                    if(native_execution.result(local_driver_attachment)) {
                        // Publish initial-match terminal before relinquishing
                        // this held frontier to its machine continuation.
                        LocalDriverFrontier(nullptr,seam);
                    }
                    return;
                }
            } else if(request.operation!=LuaDriver::Operation::Context &&
                      request.operation!=LuaDriver::Operation::Registers)
                response.error="operation not yet connected";
            local_driver_handoff.complete(std::move(response));
        }
        GFX_Events();SDL_Delay(1);
    }
}
void DEBUG_CloseLocalLuaDriver() {
    if(!local_driver_thread.joinable())return;
    local_driver_handoff.close();
    // Native guest/service frames have unwound. Revoke only this attachment's
    // grant before waking its waiter; never execute guest work or replay here.
    const auto *request=native_execution.activeRequest();
    if(!request)request=native_execution.pendingRequest();
    if(request && request->attachment.serial==local_driver_attachment.serial) {
        DEBUG_CleanupNativeExecution(*request);
        native_execution.settle(native_execution.decoderCalls(),true);
    }
    if(const auto *terminal=native_execution.result(local_driver_attachment)) {
        if(!native_execution.release(local_driver_attachment,terminal->operation))
            E_Exit("Lua driver close release failed");
    }
    local_driver_operation=0;
    while(!local_driver_handoff.isLuaClosed()) {
        // GFX_Events can throw with a read/event/verdict still awaiting native
        // consumption. Refuse reads, drain protected callback, then settle.
        LuaDriver::Request read;
        if(local_driver_handoff.takeObserverRead(read)) {
            LuaDriver::Response refused;refused.error="native lifecycle closed";
            local_driver_handoff.completeObserverRead(std::move(refused),true);
        }
        LuaDriver::Verdict verdict;
        local_driver_handoff.takeVerdict(verdict);
        LuaDriver::Response response;response.error="native lifecycle closed";
        local_driver_handoff.complete(std::move(response));
        SDL_Delay(1);
    }
    LocalDriverRetire();
}
int DEBUG_LocalLuaDriverExitStatus() {
    return local_driver_fatal || (local_driver_started && !local_driver_result.completed) ? 1 : 0;
}
void DEBUG_LocalLuaDriverShellReturned() {
    if(local_driver_selected_departed) {
        NativeFrontier::frontiers().reach(NativeFrontier::Kind::ExitFence,
                                         DOSBOX_MachineServiceOwner().token);
        // Script/worker closed at the real absence fence; this frontend has
        // no same-script relaunch and must not enter another shell decoder.
        throw 1;
    }
}
void DEBUG_LocalLuaDriverRootReturned() {
    // Normal Root return before the selected natural entry is nonarrival.
    // Called before any checkpoint disposal or worker teardown.
    if(control->lua_driver.program_set && !local_driver_started)
        E_Exit("Lua driver startup program nonarrival");
}
bool DEBUG_UnwindLocalLuaDriverFatal() {
    if(!local_driver_thread.joinable() ||
       local_driver_native_thread!=std::this_thread::get_id())return false;
    local_driver_fatal=true;
    return true;
}
void DEBUG_InstallLocalLuaDriver() {
    local_driver_native_thread=std::this_thread::get_id();
    // The shared hook is installed by DEBUG_Init, including builds without Lua.
}
#endif
Bits DEBUG_DispatchNativeExecution(void) {
    // Leave the held seam through its original continuation before activation.
    if (NativeFrontier::frontiers().holding()
#if C_DEBUG_LUA
        && !local_driver_preparing
#endif
       ) return 0;
    // Do not reenter a suspended grant's decoder tail. Unlike a boolean
    // dispatcher lifetime lock, this admits B after A has settled/released,
    // even while an old native A stack remains suspended.
    if (dispatch_operation && dispatch_operation==native_execution.operation()) return 0;
    // Consume known transport loss before activating its pending operation.
    ControlServer_RetireDeparture();
    const auto *pending=native_execution.pendingRequest();
    const bool local_boundary=pending && pending->frontend==NativeDebug::Frontend::Lua &&
        pending->completion==NativeDebug::Request::Completion::NativeBoundary;
    if (local_boundary) {
        NativeDebug::Request local;
        if (!native_execution.activate(DOSBOX_MachineServiceOwner().token,local)) return 0;
        // Only prepare the original machine continuation. No decoder/callback
        // frame or local request copy may survive into guest execution.
        if(local.over) {
#if C_DEBUG_LUA
            local.resume=StepOver(&local_driver_target_cs,&local_driver_target_ip);
            local_driver_target=local.resume;
#else
            local.resume=StepOver();
#endif
            native_execution.setContinuing(local.resume);
#if C_DEBUG_LUA
            local_driver_continuous=local.resume;
#endif
        }
        // Initial conditions run while the original stop and all native
        // resources are still untouched. The active slot permits typed reads.
#if C_DEBUG_LUA
        if(!local.stop_any.empty() && LocalDriverConditions()) {
            DEBUG_FinishNativeExecution(native_execution.operation(),0);
            return 0;
        }
#endif
        skipFirstInstruction=true;
#if C_DEBUG_LUA
        if(local.resume && local_driver_target &&
           !CBreakpoint::FindPhysBreakpoint(local_driver_target_cs,local_driver_target_ip,true))
            native_execution_breakpoint=CBreakpoint::AddBreakpoint(
                local_driver_target_cs,local_driver_target_ip,true,
                DebuggerBreakpointOwner::Internal,true,nullptr,true)->GetId();
#endif
        for(const auto &condition:local.stop_any)
            if(condition.kind==NativeDebug::StopCondition::Kind::Address)
                native_condition_breakpoints.push_back(CBreakpoint::AddBreakpoint(
                    condition.cs,condition.ip,true,DebuggerBreakpointOwner::Internal,
                    true,nullptr,true)->GetId());
        CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
        CPU_CycleLeft+=CPU_Cycles;
        CPU_Cycles=0;
        exitLoop=false;debugging=false;debug_running=false;runnormal=false;
        DOSBOX_SetNormalLoop();
        return 0;
    }
    NativeDebug::Request request;
    const bool activated=native_execution.activate(DOSBOX_MachineServiceOwner().token,request);
    if (activated && !request.stop_any.empty()) {
        native_execution.stopRuntime().begin(request.stop_any.size(),PIC_Ticks);
        if (DEBUG_EvaluateNativeConditions()) {
            DEBUG_FinishNativeExecution(native_execution.operation(),0);
            return 0;
        }
        for (const auto &condition:request.stop_any)
            if (condition.kind==NativeDebug::StopCondition::Kind::Address)
                native_condition_breakpoints.push_back(CBreakpoint::AddBreakpoint(
                    condition.cs,condition.ip,true,DebuggerBreakpointOwner::Internal,
                    true,nullptr,true)->GetId());
    }
    if (!activated) {
        const auto *active=native_execution.activeRequest();
        // Local boundary requests resume only through their original native
        // suffix. The frontier owner, never an ordinary dispatcher tail, settles.
        if(active && active->frontend==NativeDebug::Frontend::Lua &&
           active->completion==NativeDebug::Request::Completion::NativeBoundary)return 0;
        // A continuing grant can outlive this dispatcher's initial frame.
        // A nested machine invocation must not settle its ancestor's grant.
        if (!active || active->expected.invocation!=DOSBOX_MachineServiceOwner().token ||
            !debugging || debug_running) return 0;
        request=*active;
    }
    const uint64_t operation=native_execution.operation();
    struct DispatchScope {
        uint64_t &active;
        const uint64_t previous;
        DispatchScope(uint64_t &value,uint64_t operation):active(value),previous(value){active=operation;}
        ~DispatchScope(){active=previous;}
    } scope(dispatch_operation,operation);
    {
        if (!NativeControl::diagnosticEffectAllowed() || DEBUG_ContinuationUnavailable() ||
            DOSBOX_MachineServiceAdmission()!=MachineService::Denial::None) {
            DEBUG_CleanupNativeExecution(request);
            native_execution.settle(native_execution.decoderCalls(),true,0,0,0,0,0,0,NativeDebug::Terminal::ContinuationLost);
            DEBUG_ProjectNativeTerminal(request.attachment);
            E_Exit("Native execution lost its continuation");
        }
        if (request.action==NativeDebug::Request::Action::Phase) {
            // No evaluator/service frames remain. Reuse the owning instruction
            // loop: a native decoder budget is not a retired-instruction count.
            struct PhaseScope {
                PhaseScope() { native_phase_executing=true; }
                ~PhaseScope() { native_phase_executing=false; }
            } phase_scope;
            native_phase_operation=native_execution.operation();
            native_phase_result=Debugger_RunPhase(request.phase_limit,request.phase_wall_ms,
                request.phase_target,request.phase_cs,request.phase_ip);
            DEBUG_CleanupNativeExecution(request);
            native_execution.settle(native_execution.decoderCalls(),native_phase_result.event.type==DebuggerStopType::Error,
                SegValue(cs),reg_eip,reg_ax,reg_bx,reg_cx,reg_dx);
            DEBUG_ProjectNativeTerminal(request.attachment);
            DOSBOX_RequestMachineService();
            return 0;
        }
        Bits result=0;
        if (activated) try {
            native_execution.startBudget(DEBUG_ExecutionWallMilliseconds(),PIC_FullIndex());
#if C_HEAVY_DEBUG
            if (request.log) {
                DEBUG_ShowMsg("DEBUG: Starting log\n");
                cpuLogFile.open("LOGCPU.TXT");
                if (!cpuLogFile.is_open()) {
                    DEBUG_ShowMsg("DEBUG: Logfile couldn't be created.\n");
                    DEBUG_CleanupNativeExecution(request);
                    native_execution.settle(0,true,SegValue(cs),reg_eip,0,0,0,0,NativeDebug::Terminal::LogOpenError);
                    DEBUG_ProjectNativeTerminal(request.attachment);
                    if (request.frontend!=NativeDebug::Frontend::Bridge &&
                        !request.retain_result) {
                        native_execution.release(request.attachment,native_execution.result(request.attachment)->operation);
                    }
                    return 0;
                }
                cpuLogFile << hex << noshowbase << setfill('0') << uppercase;
                cpuLogType=request.log_type;
                cpuLogCounter=request.log_count;
                cpuLog=true;
                CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
                DOSBOX_SetNormalLoop();
            }
#endif
            if (request.normal_resume) {
                if (request.retrace) DEBUG_HaltOnRetrace=true;
                if (!request.watch) {
                    DrawRegistersUpdateOld();
                    debug_running=false;debugging=false;
                    DrawCode();DrawInput();
                    logBuffSuppressConsole=false;
                    if (logBuffSuppressConsoleNeedUpdate) {
                        logBuffSuppressConsoleNeedUpdate=false;
                        DEBUG_RefreshPage(0);
                    }
                    DOSBOX_SetNormalLoop();
                    GFX_SetTitle(-1,-1,-1,is_paused);
                }
                mainMenu.get_item("debugger_rundebug").check(false).refresh_item(mainMenu);
                mainMenu.get_item("debugger_runnormal").check(!request.watch).refresh_item(mainMenu);
                mainMenu.get_item("debugger_runwatch").check(request.watch).refresh_item(mainMenu);
                if (request.watch) DEBUG_DrawScreen();
            }
            if (request.action!=NativeDebug::Request::Action::Execute) {
                if (request.action==NativeDebug::Request::Action::RunInterrupt) {
                    native_execution_breakpoint=CBreakpoint::AddBreakpoint(SegValue(cs),reg_eip,true)->GetId();
                    CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip-1);
                    debugging=false;
                    DrawCode();
                    mainMenu.get_item("debugger_rundebug").check(false).refresh_item(mainMenu);
                    mainMenu.get_item("debugger_runnormal").check(true).refresh_item(mainMenu);
                    mainMenu.get_item("debugger_runwatch").check(false).refresh_item(mainMenu);
                    DOSBOX_SetNormalLoop();
                }
                CPU_HW_Interrupt(request.interrupt);
            }
            if (request.action!=NativeDebug::Request::Action::TraceInterrupt) {
            if (request.over) {
                request.resume=StepOver();
                native_execution.setContinuing(request.resume);
            }
            if (request.resume && request.frontend!=NativeDebug::Frontend::Predicate) debugging=false;
            // The shared decoder wrapper restores request flags/watch core
            // before this owner dispatches the returned callback.
            result=DEBUG_Run((int32_t)request.cycle_budget,!request.resume || request.watch);
            // Fatal diagnostic stops must unwind DEBUG_WaitNoExecute without
            // switching back into guest execution on its null decoder.
            Bits DEBUG_NullCPUCore(void);
            if (request.resume && cpudecoder==DEBUG_NullCPUCore) result=-1;
            if(!result)NativeFrontier::frontiers().reach(NativeFrontier::Kind::Decoder,
                                                       DOSBOX_MachineServiceOwner().token);
            // Consume the callback under the same active grant, after the
            // decoder returns, not in an adapter/evaluator activation.
            if (result>0) result=(Bits)DOSBOX_DispatchCallback(result);
            }
        } catch (...) {
            // Native unwind may already have settled this grant. Never clean
            // resources belonging to a later operation from an old A frame.
            if (!native_execution.ownsActive(operation)) throw;
            DEBUG_CleanupNativeExecution(request);
            debugging=true;debug_running=false;runnormal=false;
            native_execution.settle(native_execution.decoderCalls(),true,0,0,0,0,0,0,NativeDebug::Terminal::Exception);
            DEBUG_ProjectNativeTerminal(request.attachment);
            throw;
        }
        if (!native_execution.ownsActive(operation)) {
            const auto *terminal=native_execution.result(request.attachment);
            if (terminal && terminal->operation==operation && terminal->failed && !result)
                return -1;
            return result;
        }
        // Sample ceilings after the owned decoder/callback suffix as well as
        // before normal-loop work. A long single turn cannot be preempted;
        // its actual overrun remains a budget stop, not invented retirement.
        const bool budget_stop=!result && DEBUG_NativeExecutionBudgetExpired();
        // A callback may unwind the exact invocation and settle owner loss.
        // Do not overwrite that terminal fact or resume its old continuation.
        const auto *departed=native_execution.result(request.attachment);
        if (departed && departed->failed && !result) result=-1;
        if (activated && request.watch && !result && !budget_stop) {
            runnormal=false;debugging=true;debug_running=true;
            CBreakpoint::ActivateBreakpoints();
            DEBUG_DrawScreen();
        }
        // A continuing grant owns subsequent normal-loop decoder/callback
        // turns. A held stop, exit or owner unwind ends it, not the first slice.
        if (request.resume && request.frontend!=NativeDebug::Frontend::Predicate &&
            !result && (!debugging || debug_running)) return 0;
        DEBUG_FinishNativeExecution(operation,result);
        return result;
    }
    return 0;
}


uint32_t DEBUG_CheckKeys(void) {
	Bits ret=0;
	bool skipDraw = false;
#ifdef WIN32
	int key=dbg_getch_vt();
#else
	int key=getch();
#endif

    if (key == KEY_RESIZE) {
#ifdef WIN32 /* BUG: pdcurses notifies us immediately upon getting a resize event but does not update it's
                     internal structures to reflect the new console size. For example a Win32 console event
                     reporting a change to 80x41 would still return 80x40 if we asked pdcurses right now.
                     To make resizing the console window less painful, look at pdcurses internal structure
                     directly instead.

                     Note that we need to call resize_term() or pdcurses will never notify us about console
                     resize again. */
            /* FIXME: Windows will notify us if the user enlarges the window vertically, but will NOT notify
                      us if the user vertically shrinks the window (adds scrollbar instead). How do we disable
                      that behavior? */
        {
#ifdef _MSC_VER
            INPUT_RECORD *r = _pdcurses_hax_inputrecord();
            if (r->EventType == WINDOW_BUFFER_SIZE_EVENT) {
                resize_term(r->Event.WindowBufferSizeEvent.dwSize.Y, r->Event.WindowBufferSizeEvent.dwSize.X);
            }
#endif
        }
#endif
        void DEBUG_GUI_OnResize(void);
        DEBUG_GUI_OnResize();
        DEBUG_DrawScreen();
        return 0;
    }
	
	if (key >='0' && key <='5' && strlen(codeViewData.inputStr) == 0) {
		const int32_t v[] ={1,5,500,1000,5000,10000};

        NativeDebug::Request request;
        request.frontend=NativeDebug::Frontend::Interactive;
        request.attachment=DEBUG_AttachNativeExecution(request.frontend,0);
        request.expected=DEBUG_NativeExecutionContext();
        request.cycle_budget=v[key - '0'];
        if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted)
            DEBUG_ShowMsg("DEBUG: native execution admission refused\n");
        return 0;

		key = -1;
	}

	if (key>0) {
#if defined(WIN32) && defined(__PDCURSES__)
		switch (key) {
		case PADENTER:	key=0x0A;	break;
		case PADSLASH:	key='/';	break;
		case PADSTAR:	key='*';	break;
		case PADMINUS:	key='-';	break;
		case PADPLUS:	key='+';	break;
		case PADSTOP:	key=KEY_DC;		break;
		case PAD0:		key=KEY_IC;		break;
		case KEY_A1:	key=KEY_HOME;	break;
		case KEY_A2:	key=KEY_UP;		break;
		case KEY_A3:	key=KEY_PPAGE;	break;
		case KEY_B1:	key=KEY_LEFT;	break;
		case KEY_B3:	key=KEY_RIGHT;	break;
		case KEY_C1:	key=KEY_END;	break;
		case KEY_C2:	key=KEY_DOWN;	break;
		case KEY_C3:	key=KEY_NPAGE;	break;
		case ALT_D:
			if (ungetch('D') != ERR) key=27;
			break;
		case ALT_E:
			if (ungetch('E') != ERR) key=27;
			break;
		case ALT_X:
			if (ungetch('X') != ERR) key=27;
			break;
		case ALT_B:
			if (ungetch('B') != ERR) key=27;
			break;
		case ALT_S:
			if (ungetch('S') != ERR) key=27;
			break;
		}
#endif
		switch (ncurses_aware_toupper(key)) {
		case 27:	// escape (a bit slow): Clears line. and processes alt commands.
			key=getch();
			if(key < 0) { //Purely escape Clear line
				ClearInputLine();
				break;
			}

			switch(ncurses_aware_toupper(key)) {
			case 'D' : // ALT - D: DS:SI
				dataSeg = SegValue(ds);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_esi;
				else dataOfs = reg_si;
				break;
			case 'E' : //ALT - E: es:di
				dataSeg = SegValue(es);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_edi;
				else dataOfs = reg_di;
				break;
			case 'X': //ALT - X: ds:dx
				dataSeg = SegValue(ds);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_edx;
				else dataOfs = reg_dx;
				break;
			case 'B' : //ALT -B: es:bx
				dataSeg = SegValue(es);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_ebx;
				else dataOfs = reg_bx;
				break;
			case 'S': //ALT - S: ss:sp
				dataSeg = SegValue(ss);
				if (cpu.pmode && !(reg_flags & FLAG_VM)) dataOfs = reg_esp;
				else dataOfs = reg_sp;
				break;
			default:
				break;
			}
			break;
        case KEY_PPAGE: // page up
            switch (dbg.active_win) {
                case DBGBlock::WINI_CODE:
                    if (dbg.win_code != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_code,h,w);
                        win_code_ui_up(h-1);
                    }
                    break;
                case DBGBlock::WINI_DATA:
                     if (dbg.win_data != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_data,h,w);
                        win_data_ui_up(h);
                    }
                    break;
                case DBGBlock::WINI_OUT:
                    if (dbg.win_out != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_out,h,w);
                        DEBUG_RefreshPage(-h);
                    }
                    break;
            }
            break;

        case KEY_NPAGE:	// page down
            switch (dbg.active_win) {
                case DBGBlock::WINI_CODE:
                    if (dbg.win_code != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_code,h,w);
                        win_code_ui_down(h-1);
                    }
                    break;
                case DBGBlock::WINI_DATA:
                     if (dbg.win_data != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_data,h,w);
                        win_data_ui_down(h);
                    }
                    break;
                case DBGBlock::WINI_OUT:
                    if (dbg.win_out != NULL) {
                        int w,h;

                        getmaxyx(dbg.win_out,h,w);
                        DEBUG_RefreshPage(h);
                    }
                    break;
            }
            break;

		case KEY_DOWN:	// down
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        win_code_ui_down(1);
                        break;
                    case DBGBlock::WINI_DATA:
                        win_data_ui_down(1);
                        break;
                    case DBGBlock::WINI_OUT:
                        DEBUG_RefreshPage(1);
                        break;
                }
                break;
        case KEY_UP:	// up 
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        win_code_ui_up(1);
                        break;
                    case DBGBlock::WINI_DATA:
                        win_data_ui_up(1);
                        break;
                    case DBGBlock::WINI_OUT:
                        DEBUG_RefreshPage(-1);
                        break;
                }
				break;

		case KEY_HOME:	// Home
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        // and do what?
                        break;
                    case DBGBlock::WINI_DATA:
                        // and do what?
                        break;
                    case DBGBlock::WINI_OUT:
                        void DEBUG_ScrollHomeOutput(void);
                        DEBUG_ScrollHomeOutput();
                        break;
                }
				break;

		case KEY_END:	// End
                switch (dbg.active_win) {
                    case DBGBlock::WINI_CODE:
                        // and do what?
                        break;
                    case DBGBlock::WINI_DATA:
                        // and do what?
                        break;
                    case DBGBlock::WINI_OUT:
                        void DEBUG_ScrollToEndOutput(void);
                        DEBUG_ScrollToEndOutput();
                        break;
                }
				break;

        case KEY_IC:	// Insert: toggle insert/overwrite
				codeViewData.ovrMode = !codeViewData.ovrMode;
				break;
		case KEY_LEFT:	// move to the left in command line
				if (codeViewData.inputPos > 0) codeViewData.inputPos--;
 				break;
		case KEY_RIGHT:	// move to the right in command line
				if (codeViewData.inputStr[codeViewData.inputPos]) codeViewData.inputPos++;
				break;
		case KEY_F(6):	// previous command (f1-f4 generate rubbish at my place)
		case KEY_F(3):	// previous command
				if (histBuffPos == histBuff.begin()) break;
				if (histBuffPos == histBuff.end()) {
					// copy inputStr to suspInputStr so we can restore it
					safe_strncpy(codeViewData.suspInputStr, codeViewData.inputStr, sizeof(codeViewData.suspInputStr));
				}
				safe_strncpy(codeViewData.inputStr,(*--histBuffPos).c_str(),sizeof(codeViewData.inputStr));
				codeViewData.inputPos = (int)strlen(codeViewData.inputStr);
				break;
		case KEY_F(7):	// next command (f1-f4 generate rubbish at my place)
		case KEY_F(4):	// next command
				if (histBuffPos == histBuff.end()) break;
				if (++histBuffPos != histBuff.end()) {
					safe_strncpy(codeViewData.inputStr,(*histBuffPos).c_str(),sizeof(codeViewData.inputStr));
				} else {
					// copy suspInputStr back into inputStr
					safe_strncpy(codeViewData.inputStr, codeViewData.suspInputStr, sizeof(codeViewData.inputStr));
				}
				codeViewData.inputPos = (int)strlen(codeViewData.inputStr);
				break;
		case KEY_F(5): { // Run Program
                if (!DEBUG_ResumeNativeExecution(NativeDebug::Frontend::Interactive,false,false,DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0)))
                    DEBUG_ShowMsg("DEBUG: native execution admission refused\n");
                return 0;
            }
		case KEY_F(8):	// Toggle printable characters
				showPrintable = !showPrintable;
				break;
		case KEY_F(9):	// Set/Remove Breakpoint
                if (!DEBUG_AdmitNativeEffect(DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0))) {
                    DEBUG_ShowMsg("DEBUG: breakpoint admission refused\n");
                    break;
                }
				if (CBreakpoint::IsBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs)) {
					if (CBreakpoint::DeleteBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs))
						DEBUG_ShowMsg("DEBUG: Breakpoint deletion success.\n");
					else
						DEBUG_ShowMsg("DEBUG: Failed to delete breakpoint.\n");
				}
				else {
					CBreakpoint::AddBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs, false);
					DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X\n",codeViewData.cursorSeg,codeViewData.cursorOfs);
				}
				break;
		case KEY_F(10): { // Step over inst
                NativeDebug::Request request;
                request.frontend=NativeDebug::Frontend::Interactive;
        request.attachment=DEBUG_AttachNativeExecution(request.frontend,0);
                request.expected=DEBUG_NativeExecutionContext();
                request.over=true;
                if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted) {
                    DEBUG_ShowMsg("DEBUG: native execution admission refused\n");
                    return 0;
                }
				DrawRegistersUpdateOld();
                if(!CPU_IsDynamicCore()) warn_dynamic = false;
                else if(!warn_dynamic) {
                    DEBUG_WarnDynamic();
                    warn_dynamic = true;
                }
                break;
            }
		case KEY_F(11): { // trace into
                NativeDebug::Request request;
                request.frontend=NativeDebug::Frontend::Interactive;
        request.attachment=DEBUG_AttachNativeExecution(request.frontend,0);
                request.expected=DEBUG_NativeExecutionContext();
                if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted) {
                    DEBUG_ShowMsg("DEBUG: native execution admission refused\n");
                    return 0;
                }
				DrawRegistersUpdateOld();
                if(!CPU_IsDynamicCore()) warn_dynamic = false;
                else if(!warn_dynamic) {
                    DEBUG_WarnDynamic();
                    warn_dynamic = true;
                }
				exitLoop = false;
                break;
            }
        case 0x09: //TAB
                void DBGUI_NextWindow(void);
                DBGUI_NextWindow();
                break;
        case KEY_BTAB:
                void DBGUI_PrevWindow(void);
                DBGUI_PrevWindow();
                break;
		case 0x0A: //Parse typed Command
                if (codeViewData.inputPos > 0) {
                    codeViewData.inputStr[MAXCMDLEN] = '\0';
                    if(DEBUG_ExecuteSessionCommand(codeViewData.inputStr,
                            DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0))) {
                        char* cmd = ltrim(codeViewData.inputStr);
                        if (*cmd && (histBuff.empty() || *--histBuff.end()!=cmd))
                            histBuff.emplace_back(cmd);
                        if (histBuff.size() > MAX_HIST_BUFFER) histBuff.pop_front();
                        histBuffPos = histBuff.end();
                        ClearInputLine();
                    } else {
                        DEBUG_ShowMsg("*** Debugger command not recognized");
                        codeViewData.inputPos = (int)strlen(codeViewData.inputStr);
                    }
                }
				break;
		case KEY_BACKSPACE: //backspace (linux)
		case 0x7f:	// backspace in some terminal emulators (linux)
		case 0x08:	// delete 
				if (codeViewData.inputPos == 0) break;
				codeViewData.inputPos--;
				// fallthrough
		case KEY_DC: // delete character
				if ((codeViewData.inputPos<0) || (codeViewData.inputPos>=MAXCMDLEN)) break;
				if (codeViewData.inputStr[codeViewData.inputPos] != 0) {
						codeViewData.inputStr[MAXCMDLEN] = '\0';
						for(char* p=&codeViewData.inputStr[codeViewData.inputPos];(*p=*(p+1));p++) {}
				}
 				break;
		default:
				if ((key>=32) && (key<127)) {
					if ((codeViewData.inputPos<0) || (codeViewData.inputPos>=MAXCMDLEN)) break;
					codeViewData.inputStr[MAXCMDLEN] = '\0';
					if (codeViewData.inputStr[codeViewData.inputPos] == 0) {
							codeViewData.inputStr[codeViewData.inputPos++] = char(key);
							codeViewData.inputStr[codeViewData.inputPos] = '\0';
					} else if (!codeViewData.ovrMode) {
						int len = (int) strlen(codeViewData.inputStr);
						if (len < MAXCMDLEN) {
							for(len++;len>codeViewData.inputPos;len--)
								codeViewData.inputStr[len]=codeViewData.inputStr[len-1];
							codeViewData.inputStr[codeViewData.inputPos++] = char(key);
						}
					} else {
						codeViewData.inputStr[codeViewData.inputPos++] = char(key);
					}
				} else if (key==killchar()) {
					ClearInputLine();
				}
				break;
		}
        // Do not consume another command while a decoded callback still owns
        // the native suffix. RunMachine dispatches it after this frame returns.
        if (native_execution.state()==NativeDebug::State::Pending) return 0;
		if (ret) return (uint32_t)ret;
		ret=0;
		if (!skipDraw)
			DEBUG_DrawScreen();
	}
	return (uint32_t)ret;
}

Bitu DEBUG_LastRunningUpdate = 0;

LoopHandler *DOSBOX_GetLoop(void);
Bitu DEBUG_Loop(void);

Bits DEBUG_NullCPUCore(void) {
    return CBRET_NONE;
}

void DEBUG_WaitNoExecute(void) {
    /* the caller uses this version to indicate a fatal error
     * in a condition where single-stepping or executing any
     * more x86 instructions is very unwise */
    // Fatal diagnostic UI, not a guest invocation: do not recursively run
    // the machine dispatcher/service tail against corrupted DOS state.
    struct RestoreDecoder {
        decltype(cpudecoder) saved;
        ~RestoreDecoder() { cpudecoder = saved; }
    } restore{cpudecoder};
    cpudecoder = DEBUG_NullCPUCore;
    while (DOSBOX_GetLoop() == DEBUG_Loop) {
        if (DEBUG_Loop()) break;
    }
}

int GetDynamicType();

#if (C_DYNAMIC_X86)
void dyn_core_dh_debug_flush (void);
#endif

Bitu DEBUG_Loop(void) {
    if (DEBUG_NativeExecutionBudgetExpired()) return 0;
    // Publish the held stop at the outer seam before accepting another command.
    if (native_execution.state()==NativeDebug::State::Active && debugging && !debug_running)
        return 0;
    if (DEBUG_ContinuationUnavailable()) {
        GFX_Events();
        SDL_Delay(1);
        return 0;
    }
#if C_DEBUG_LUA
    if(Debugger_PredicateHeld()) {
        // Before PIC, GFX, Lua pump and native debugger command processing.
        if(predicate_hold==PredicateHold::Unsupported || predicate_hold==PredicateHold::Pending) {
            if(predicate_hold==PredicateHold::Unsupported && !predicate_hold_reported &&
               Debugger_LiveHostNow()-predicate_hold_start>=100) {
                predicate_hold_reported=true;
                const auto now=Debugger_GetRegisters();
                const auto &before=predicate_registers;
                const bool regs_unchanged=now.eax==before.eax && now.ebx==before.ebx &&
                    now.ecx==before.ecx && now.edx==before.edx && now.esi==before.esi &&
                    now.edi==before.edi && now.ebp==before.ebp && now.esp==before.esp &&
                    now.eip==before.eip && now.cs==before.cs && now.ds==before.ds &&
                    now.es==before.es && now.fs==before.fs && now.gs==before.gs &&
                    now.ss==before.ss && now.flags==before.flags;

                fprintf(stderr,"hook-unsupported-held id=%llu cs=%04x ip=%08x tick_before=%llu tick_now=%llu tasks_unchanged=%u regs_unchanged=%u\n",
                    (unsigned long long)predicate_selected_id,SegValue(cs),reg_eip,
                    (unsigned long long)predicate_tick,(unsigned long long)PIC_Ticks,unsigned(Debugger_PredicateTasksUnchanged()),unsigned(regs_unchanged));
            }
            SDL_Delay(1);return 0;
        }
        Debugger_ExecutionMaintain(); // absolute deadline is not paused
        const bool channel_healthy=Debugger_LuaPredicateChannelHealthy();
        if(channel_healthy && Debugger_LuaPredicateRequestPending())return 0;
        const bool suppress=channel_healthy && predicate_hold==PredicateHold::Suppress &&
            DOSBOX_MachineServiceOwner().token==predicate_owner.token &&
            DOSBOX_MachineServiceAdmission()==MachineService::Denial::None &&
            cpudecoder==CPU_Core_Normal_Run;
        PredicateCount(suppress?predicate_summary.suppressed:predicate_summary.propagated);
        PredicateReceipt(suppress?"suppressed":(*predicate_summary.last.diagnostic?"error":"propagated"),!suppress);
        predicate_hold=PredicateHold::None;
        if(!suppress) {
            DEBUG_EnableDebugger();
            // Genuine predicate propagation uses the existing task fault cleanup;
            // release owned edges without advancing guest or executing suffix.
            GFX_FaultLuaTasks();GFX_PumpLuaTasks(false,true);
            return 0;
        }
        if (!DEBUG_DeferPredicateInstruction())
            E_Exit("Predicate continuation could not be deferred");
        debugging=false;debug_running=false;runnormal=true;
        return 0;
    }
#endif
#if defined(C_DOSBOX_AGENT)
    dosbox_agent::AGENT_BridgePump();
    if (native_execution.state()==NativeDebug::State::Pending) return 0;
#endif
    ControlServer_Poll();
    if (native_execution.state()==NativeDebug::State::Pending) return 0;

    // MCP commands such as RUN or VRT can switch back to the normal loop.
    if (DOSBOX_GetLoop() != DEBUG_Loop)
        return 0;

    if (debug_running) {
        Bitu now = SDL_GetTicks();

        if ((DEBUG_LastRunningUpdate + 33) < now) {
            DEBUG_LastRunningUpdate = now;
            SetCodeWinStart();
            DEBUG_DrawScreen();
        }

        return old_loop();
    }
    else {
#if C_DEBUG_LUA
        // In configured live mode a genuine stop must not execute PIC IRQs
        // or reenter guest while waiting for read-only service at its owner.
        const auto owner=DOSBOX_MachineServiceOwner();
        if(Debugger_LuaLiveConfigured() && owner.token &&
           (owner.kind==MachineService::Kind::RootGuest || owner.kind==MachineService::Kind::ShellExecGuest)) {
            Debugger_EntryMaintain();
    GFX_Events();Debugger_LuaLivePump();SDL_Delay(1);return 0;
        }
#endif
        //TODO Disable sound
        GFX_Events();
        if (native_execution.state()==NativeDebug::State::Pending) return 0;
        // Held means no guest work. Host input may enqueue an explicit native
        // execution request, but must not inject an IRQ or implicitly resume
        // the guest to return from one. Pending IRQs belong to the next grant.
        SDL_Delay(1);

#if (C_DYNAMIC_X86)
	if (GetDynamicType() > 0) {
		/* Force dynamic core to flush whatever internal FPU state to the FPU state we can see */
		dyn_core_dh_debug_flush();
	}
#endif

        /* between DEBUG_Enable and DEBUG_Loop CS:EIP can change */
        if (check_rescroll) {
            uint16_t ocs;
            uint32_t oip;
            int ocr;

            check_rescroll = false;
            ocs = codeViewData.useCS;
            oip = codeViewData.useEIP;
            ocr = codeViewData.cursorPos;
            SetCodeWinStart();
            if (ocs != codeViewData.useCS ||
                    oip != codeViewData.useEIP) {
                DEBUG_DrawScreen();
            }
            else {
                /* SetCodeWinStart() resets cursor position */
                codeViewData.cursorPos = ocr;
            }
        }

        if (logBuffSuppressConsoleNeedUpdate) {
            logBuffSuppressConsoleNeedUpdate = false;
            DEBUG_RefreshPage(0);
        }

    	return DEBUG_CheckKeys();
    }
}

void DEBUG_FlushInput(void);

void DEBUG_WarnDynamic(void) {
    DEBUG_ShowMsg("Warning: Single-stepping may not work correctly with \"Dynamic core\".");
    DEBUG_ShowMsg("         If you encounter problems, switch the CPU core to \"Normal core\".");
}

bool tohide=true;
static bool hidedebugger=false;
void DEBUG_Enable_Handler(bool pressed) {
	if (!pressed || control->opt_display2)
		return;
#if C_DEBUG_LUA
    // The selected worker has no interactive tty or UI toggle. A host break
    // publishes a native stop even while an observer is deciding its verdict;
    // the hit stack's exitLoop check preserves it against a false verdict.
    if(local_driver_thread.joinable()) {
        local_driver_host_break=true;
        DEBUG_HoldNativeExecution();
        return;
    }
#endif

    if (hidedebugger) {
        hidedebugger=false;
#if defined(WIN32) && !defined(_WIN32_WINDOWS)
        ShowWindow(GetConsoleWindow(), SW_SHOW);
#endif
    } else if (tohide && (runnormal||debug_running||debugging)) {
        hidedebugger=true;
#if defined(WIN32) && !defined(_WIN32_WINDOWS)
        ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
        debugrunmode=debuggerrun;
        mainMenu.get_item("mapper_debugger").check(false).refresh_item(mainMenu);
        mainMenu.get_item("debugger_rundebug").check(debugrunmode==0).refresh_item(mainMenu);
        mainMenu.get_item("debugger_runnormal").check(debugrunmode==1).refresh_item(mainMenu);
        mainMenu.get_item("debugger_runwatch").check(debugrunmode==2).refresh_item(mainMenu);
        if (runnormal) return;
    }

    /* this command is now a toggle! */
    /* However this should break back into the debugger if RUNWATCH is active */
    if (debug_running) {
        debug_running = false;
        DrawRegistersUpdateOld();
        SetCodeWinStart();
        DEBUG_DrawScreen();
        if (tohide&&!debugging) return;
    }

    if (debugging) {
        // Toggling a held debugger is an execution request, not permission to
        // switch loops on the input handler's stack. Preserve Busy refusal.
        if (!DEBUG_ResumeNativeExecution(NativeDebug::Frontend::Interactive,false,false,DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0)))
            DEBUG_ShowMsg("DEBUG: native execution admission refused\n");
        return;
    }

	static bool showhelp=false;

#if defined(MACOSX) || defined(LINUX)
	/* Mac OS X does not have a console for us to just allocate on a whim like Windows does.
	   So the debugger interface is useless UNLESS the user has started us from a terminal
	   (whether over SSH or from the Terminal app). */
    bool allow = true;

    if (!isatty(0) || !isatty(1) || !isatty(2))
	    allow = false;

    if (!allow) {
# if defined(MACOSX)
	    LOG_MSG("Debugger in Mac OS X is not available unless you start DOSBox-X from a terminal or from the Terminal application");
# else
	    LOG_MSG("Debugger is not available unless you start DOSBox-X from a terminal");
# endif
	    return;
    }
#endif

    CPU_CycleLeft+=CPU_Cycles;
    CPU_Cycles=0;

    logBuffSuppressConsole = true;

    LoopHandler *ol = DOSBOX_GetLoop();
    if (ol != DEBUG_Loop) old_loop = ol;

    DEBUG_EnterCommonStoppedState();
    check_rescroll=true;
    DrawRegistersUpdateOld();
    DEBUG_SetupConsole();
    DEBUG_FlushInput();
	SetCodeWinStart();
	DEBUG_DrawScreen();
	DOSBOX_SetLoop(&DEBUG_Loop);
	mainMenu.get_item("mapper_debugger").check(true).refresh_item(mainMenu);
    if(!CPU_IsDynamicCore()) warn_dynamic = false;
    else if(!warn_dynamic) {
        DEBUG_WarnDynamic();
        warn_dynamic = true;
    }
    if (!showhelp) {
        showhelp=true;
		DEBUG_ShowMsg("***| TYPE HELP (+ENTER) TO GET AN OVERVIEW OF ALL COMMANDS |***\n");
	}
	//KEYBOARD_ClrBuffer();
    GFX_SetTitle(-1,-1,-1,false);
    runnormal = false;
#if defined(C_DOSBOX_AGENT)
    dosbox_agent::AGENT_NotifyDebuggerStopped(SegValue(cs), reg_eip);
#endif
    if (debugrunmode==1 || debugrunmode==2)
        DEBUG_ExecuteSessionCommand(debugrunmode==1?"RUN":"RUNWATCH",
            DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0));
}

void DEBUG_DrawScreen(void) {
	DrawData();
	DrawCode();
    DrawInput();
	DrawRegisters();
	DrawVariables();
}

static void DEBUG_RaiseTimerIrq(void) {
	PIC_ActivateIRQ(0);
}

static void LogDEVChain(uint32_t devhdr) {
	DOS_DEVHDR::hdr hdr;
	char tmp[9];

	while (1) {
		MEM_BlockRead(PhysMake(devhdr >> 16,devhdr & 0xFFFFu),&hdr,sizeof(hdr));
		memcpy(tmp,hdr.name,8); tmp[8] = 0;
		DEBUG_ShowMsg("%04X:%04X %04X  %04X  %04X  %s",
			devhdr >> 16,devhdr & 0xFFFFu,
			hdr.attributes,hdr.strategy_entry,hdr.interrupt_entry,tmp);

		devhdr = hdr.nextdev;
		if (devhdr == NONEXTDEV) break;

		/* Apparently in MS-DOS 5, a driver can set only the offset field to 0xFFFF and that is sufficient to end the linked list */
		if ((devhdr&0xFFFFu) == 0xFFFFu) break;
	}
}

// Display the content of the MCB chain starting with the MCB at the specified segment.
static void LogMCBChain(uint16_t mcb_segment) {
	std::string filename;
	const char *psp_seg_note;
	uint16_t DOS_dataOfs = static_cast<uint16_t>(dataOfs); //Realmode addressing only
	PhysPt dataAddr = PhysMake(dataSeg,DOS_dataOfs);// location being viewed in the "Data Overview"
	uint16_t end_of_chain_segment = mcb_segment;

	for (const auto mcb : DOS_MCB(mcb_segment)) {
		const auto current_segment = mcb.GetSeg();

		// verify that the type field is valid
		if (!mcb.isValid()) {
			DEBUG_ShowMsg("MCB chain broken at %04X:0000!",current_segment);
			return;
		}

		filename = mcb.GetFileName();

		// some PSP segment values have special meanings
		switch (mcb.GetPSPSeg()) {
			case MCB_FREE:
				psp_seg_note = "(free)";
				break;
			case MCB_DOS:
				psp_seg_note = "(DOS)";
				break;
			default:
				psp_seg_note = "";
		}

		DEBUG_ShowMsg("   %04X  %12u     %04X %-7s  %s",current_segment,mcb.GetSize() << 4,mcb.GetPSPSeg(), psp_seg_note, filename.c_str());

		// print a message if dataAddr is within this MCB's memory range
		PhysPt mcbStartAddr = PhysMake(current_segment+1,0);
		PhysPt mcbEndAddr = PhysMake(current_segment+1+mcb.GetSize(),0);
		if (dataAddr >= mcbStartAddr && dataAddr < mcbEndAddr) {
			DEBUG_ShowMsg("   (data addr %04hX:%04X is %u bytes past this MCB)",dataSeg,DOS_dataOfs,dataAddr - mcbStartAddr);
		}
		end_of_chain_segment = static_cast<uint16_t>(current_segment + mcb.GetSize() + 1);
	}

	DEBUG_ShowMsg("   %04X  END OF CHAIN",end_of_chain_segment);
}

#include "regionalloctracking.h"

extern bool dos_kernel_disabled;
extern RegionAllocTracking rombios_alloc;

static void LogBIOSMem(void) {
    char tmp[192];

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("BIOS memory blocks:");
    DEBUG_ShowMsg("Region            Status What");
    for (auto i=rombios_alloc.alist.begin();i!=rombios_alloc.alist.end();i++) {
        sprintf(tmp,"%08lx-%08lx %s %s",
            (unsigned long)(i->start),
            (unsigned long)(i->end),
            i->free ? "FREE  " : "ALLOC ",
	    i->free ? "    " : (i->fixed ? "FIX "  : "DYN "));
        DEBUG_ShowMsg("%s %s",tmp,i->who.c_str());
    }

    DEBUG_EndPagedContent();
}

Bitu XMS_GetTotalHandles(void);
bool XMS_GetHandleInfo(Bitu &phys_location,Bitu &size,Bitu &lockcount,bool &free,Bitu handle);

#if !defined(OSFREE)
bool EMS_GetHandle(Bitu &size,PhysPt &addr,std::string &name,Bitu handle);
const char *EMS_Type_String(void);
Bitu EMS_Max_Handles(void);
bool EMS_Active(void);
#endif

static void LogFNKEY(void) {
    DEBUG_BeginPagedContent();

    void DEBUG_INTDC_FnKeyMapInfo(void);
    DEBUG_INTDC_FnKeyMapInfo();

    DEBUG_EndPagedContent();
}

#if !defined(OSFREE)
static void LogEMS(void) {
    Bitu h_size;
    PhysPt xh_addr;
    std::string h_name;

    if (dos_kernel_disabled) {
        DEBUG_ShowMsg("Cannot enumerate EMS memory while DOS kernel is inactive.");
        return;
    }

    if (!EMS_Active()) {
        DEBUG_ShowMsg("Cannot enumerate EMS memory while EMS is inactive.");
        return;
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("EMS memory (type %s) handles:",EMS_Type_String());
    DEBUG_ShowMsg("Handle Address  Size (bytes)    Name");
    for (Bitu h=0;h < EMS_Max_Handles();h++) {
        if (EMS_GetHandle(/*&*/h_size,/*&*/xh_addr,/*&*/h_name,h)) {
            DEBUG_ShowMsg("%6lu %08lx %08lx %s",
                (unsigned long)h,
                (unsigned long)xh_addr,
                (unsigned long)h_size,
                h_name.c_str());
        }
    }

    bool EMS_GetMapping(Bitu &handle,uint16_t &log_page,Bitu ems_page);
    Bitu GetEMSPageFrameSegment(void);
    Bitu GetEMSPageFrameSize(void);

    DEBUG_ShowMsg("EMS page frame 0x%08lx-0x%08lx",
        (long unsigned int)(GetEMSPageFrameSegment()*16UL),
        (long unsigned int)((GetEMSPageFrameSegment()*16UL)+GetEMSPageFrameSize()-1UL));
    DEBUG_ShowMsg("Handle Page(p/l) Address");

    for (Bitu p=0;p < (GetEMSPageFrameSize() >> 14UL);p++) {
        uint16_t log_page;
        Bitu handle;

        if (EMS_GetMapping(handle,log_page,p)) {
            char tmp[192] = {0};

            xh_addr = 0;
            h_size = 0;
            h_name.clear();
            EMS_GetHandle(/*&*/h_size,/*&*/xh_addr,/*&*/h_name,handle);

            if (xh_addr != 0)
                sprintf(tmp," virt -> %08lx-%08lx phys",
                    (unsigned long)(xh_addr + ((PhysPt)log_page << 14u)),
                    (unsigned long)(xh_addr + ((PhysPt)log_page << 14u) + (1u << 14u) - 1u));

            DEBUG_ShowMsg("%6lu %4lu/%4lu %08lx-%08lx%s",(unsigned long)handle,
                (unsigned long)p,(unsigned long)log_page,
                (long unsigned int)((GetEMSPageFrameSegment()*16UL)+(p << 14UL)),
                (long unsigned int)((GetEMSPageFrameSegment()*16UL)+((p+1UL) << 14UL)-1),
                tmp);
        }
        else {
            DEBUG_ShowMsg("--     %4lu/     %08lx-%08lx",(unsigned long)p,
                (long unsigned int)((GetEMSPageFrameSegment()*16UL)+(p << 14UL)),
                (long unsigned int)((GetEMSPageFrameSegment()*16UL)+((p+1UL) << 14UL)-1));
        }
    }

    DEBUG_EndPagedContent();
}
#endif

static void LogXMS(void) {
    Bitu phys_location;
    Bitu lockcount;
    bool free;
    Bitu size;

    if (dos_kernel_disabled) {
        DEBUG_ShowMsg("Cannot enumerate XMS memory while DOS kernel is inactive.");
        return;
    }

    if (!XMS_Active()) {
        DEBUG_ShowMsg("Cannot enumerate XMS memory while XMS is inactive.");
        return;
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("XMS memory handles:");
    DEBUG_ShowMsg("Handle Status Location Size (bytes)");
    for (Bitu h = 1; h < XMS_GetTotalHandles(); h++) {
        if (XMS_GetHandleInfo(/*&*/phys_location,/*&*/size,/*&*/lockcount,/*&*/free, h)) {
            if (!free || phys_location != 0 || size != 0) {
                DEBUG_ShowMsg("%6lu %s 0x%08lx %lu",
                        (unsigned long)h,
                        free ? "FREE " : "ALLOC ",
                        (unsigned long)phys_location,
                        (unsigned long)size << 10UL); /* KB -> bytes */
            }
        }
    }

    DEBUG_EndPagedContent();
}

static void LogDOSKernMem(void) {
    char tmp[192];

    if (dos_kernel_disabled) {
        DEBUG_ShowMsg("Cannot enumerate DOS kernel memory while DOS kernel is inactive.");
        return;
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("DOS kernel memory blocks:");
    DEBUG_ShowMsg("Seg      Size (bytes)     What");
    for (auto i=DOS_GetMemLog.begin();i!=DOS_GetMemLog.end();i++) {
        sprintf(tmp,"%04x     %8lu     ",
                (unsigned int)(i->segbase),
                (unsigned long)(i->pages << 4UL));

        DEBUG_ShowMsg("%s    %s",tmp,i->who.c_str());
    }

    DEBUG_EndPagedContent();
}

// Display the content of all device drivers.
static void LogDEVS(void) {
	if (dos_kernel_disabled) {
		if (boothax == BOOTHAX_MSDOS) {
			if (guest_msdos_LoL == 0 || guest_msdos_dev_chain == 0) {
				DEBUG_ShowMsg("Cannot enumerate device list while DOS kernel is inactive, and DOSBox-X has not yet determined the DEV list of the guest MS-DOS operating system");
				return;
			}

			DEBUG_BeginPagedContent();

			try {
				DEBUG_ShowMsg("Header    Attr  Strat Intr  Name");
				LogDEVChain(guest_msdos_dev_chain);
			}
			catch (GuestPageFaultException &pf) {
				(void)pf;//unused
				DEBUG_ShowMsg("(Enumeration caused page fault within the guest)");
			}

			DEBUG_EndPagedContent();
			return;
		}
		else {
			DEBUG_ShowMsg("Cannot enumerate device list while DOS kernel is inactive.");
			return;
		}
	}

	DEBUG_BeginPagedContent();

	DEBUG_ShowMsg("Header    Attr  Strat Intr  Name");
	LogDEVChain(dos_infoblock.GetStartOfDeviceChain());

	DEBUG_EndPagedContent();
}

// Display the content of all Memory Control Blocks.
static void LogMCBS(void) {
    if (dos_kernel_disabled) {
        if (boothax == BOOTHAX_MSDOS) {
            if (guest_msdos_LoL == 0 || guest_msdos_mcb_chain == 0) {
                DEBUG_ShowMsg("Cannot enumerate MCB list while DOS kernel is inactive, and DOSBox-X has not yet determined the MCB list of the guest MS-DOS operating system");
                return;
            }

            DEBUG_BeginPagedContent();

            try {
                DEBUG_ShowMsg("MCB Seg  Size (bytes)  PSP Seg (notes)  Filename");
                DEBUG_ShowMsg("Conventional memory:");
                LogMCBChain(guest_msdos_mcb_chain);
            }
            catch (GuestPageFaultException &pf) {
                (void)pf;//unused
                DEBUG_ShowMsg("(Enumeration caused page fault within the guest)");
            }

            DEBUG_EndPagedContent();
            return;
        }
        else {
            DEBUG_ShowMsg("Cannot enumerate MCB list while DOS kernel is inactive.");
            return;
        }
    }

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("MCB Seg  Size (bytes)  PSP Seg (notes)  Filename");
    DEBUG_ShowMsg("Conventional memory:");
    LogMCBChain(dos.firstMCB);

    if (dos_infoblock.GetStartOfUMBChain() != 0xFFFF) {
        DEBUG_ShowMsg("Upper memory:");
        LogMCBChain(dos_infoblock.GetStartOfUMBChain());
    }

    DEBUG_EndPagedContent();
}

static void LogGDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu length = cpu.gdt.GetLimit();
	PhysPt address = cpu.gdt.GetBase();
	PhysPt max	   = (PhysPt)(address + length);
	Bitu i = 0;

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("GDT Base:%08lX Limit:%08lX",(unsigned long)address,(unsigned long)length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08lX type: %02X parbg",(int)(i<<3),(unsigned long)desc.GetBase(),desc.saved.seg.type);
        DEBUG_ShowMsg("%s",out1);
		sprintf(out1,"      l:%08lX dpl : %01X  %1X%1X%1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
        DEBUG_ShowMsg("%s",out1);
		address+=8; i++;
	}

    DEBUG_EndPagedContent();
}

static void LogLDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu ldtSelector = cpu.gdt.SLDT();
	if (!cpu.gdt.GetDescriptor(ldtSelector,desc)) return;
	Bitu length = desc.GetLimit();
	PhysPt address = desc.GetBase();
	PhysPt max	   = (PhysPt)(address + length);
	Bitu i = 0;

    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("LDT Base:%08lX Limit:%08lX",(unsigned long)address,(unsigned long)length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08lX type: %02X parbg",(int)((i<<3)|4),(unsigned long)desc.GetBase(),desc.saved.seg.type);
        DEBUG_ShowMsg("%s",out1);
		sprintf(out1,"      l:%08lX dpl : %01X  %1X%1X%1X%1X%1X",(unsigned long)desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
        DEBUG_ShowMsg("%s",out1);
		address+=8; i++;
	}

    DEBUG_EndPagedContent();
}

static void LogIDT(void) {
	char out1[512];
	Descriptor desc;
	Bitu address = 0;

    DEBUG_BeginPagedContent();

	while (address<256*8) {
		if (cpu.idt.GetDescriptor(address,desc)) {
			sprintf(out1,"%04X: sel:%04X off:%02X",(unsigned int)(address/8),(int)desc.GetSelector(),(int)desc.GetOffset());
            DEBUG_ShowMsg("%s",out1);
		}
		address+=8;
	}

    DEBUG_EndPagedContent();
}

void LogPages(char* selname) {
    DEBUG_BeginPagedContent();

    if (paging.enabled) {
	    char out1[512];
	    Bitu sel = GetHexValue(selname,selname);
	    if ((sel==0x00) && ((*selname==0) || (*selname=='*'))) {
		    for (unsigned int i=0; i<0xfffff; i++) {
			    Bitu table_addr=(paging.base.page<<12u)+(i >> 10u)*(Bitu)4u;
			    X86PageEntry table;
			    table.load=phys_readd((PhysPt)table_addr);
			    if (table.block.p) {
				    X86PageEntry entry;
				    PhysPt entry_addr=(table.block.base<<12u)+(i & 0x3ffu)* 4u;
				    entry.load=phys_readd(entry_addr);
				    if (entry.block.p) {
					    sprintf(out1,"page %05Xxxx -> %04Xxxx  flags [uw] %x:%x::%x:%x [d=%x|a=%x]",
							    i,entry.block.base,entry.block.us,table.block.us,
							    entry.block.wr,table.block.wr,entry.block.d,entry.block.a);
					    DEBUG_ShowMsg("%s",out1);
				    }
			    }
		    }
	    } else {
		    Bitu table_addr=(paging.base.page<<12u)+(sel >> 10u)*4u;
		    X86PageEntry table;
		    table.load=phys_readd((PhysPt)table_addr);
		    if (table.block.p) {
			    X86PageEntry entry;
			    Bitu entry_addr=((Bitu)table.block.base<<12u)+(sel & 0x3ffu)*4u;
			    entry.load=phys_readd((PhysPt)entry_addr);
			    sprintf(out1,"page %05lXxxx -> %04lXxxx  flags [puw] %x:%x::%x:%x::%x:%x",
					    (unsigned long)sel,
					    (unsigned long)entry.block.base,
					    entry.block.p,table.block.p,entry.block.us,table.block.us,entry.block.wr,table.block.wr);
			    DEBUG_ShowMsg("%s",out1);
		    } else {
			    sprintf(out1,"pagetable %03X not present, flags [puw] %x::%x::%x",
					    (int)(sel >> 10),
					    (int)table.block.p,
					    (int)table.block.us,
					    (int)table.block.wr);
			    DEBUG_ShowMsg("%s",out1);
		    }
	    }
    }

    DEBUG_EndPagedContent();
}

const char *FPU_tag(bool regvalid)
{
    return regvalid ? "valid" : "empty";
}

static void LogFPUInfo(void) {
    DEBUG_BeginPagedContent();

    DEBUG_ShowMsg("status: %s", fpu.sw.to_string().c_str());

    for (unsigned int i=0;i < 8;i++) {
        unsigned int adj = FPU_StackIndex(i);

#if C_FPU_X86 && HAS_LONG_DOUBLE
        DEBUG_ShowMsg(" st(%u): %s val=%.20Lg (0x%04x%08x%08x)", i, FPU_tag(fpu.regvalid[adj]),
                      reinterpret_cast<long double&>(fpu.p_regs[adj]), fpu.p_regs[adj].m3,
                      fpu.p_regs[adj].m2, fpu.p_regs[adj].m1);
#elif C_FPU_X86
        DEBUG_ShowMsg(" st(%u): %s val=0x%04x%08x%08x", i, FPU_tag(fpu.regvalid[adj]),
                      fpu.p_regs[adj].m3, fpu.p_regs[adj].m2, fpu.p_regs[adj].m1);
#elif HAS_LONG_DOUBLE
        DEBUG_ShowMsg(" st(%u): %s val=%.20Lg (0x%04x%016llx)", i, FPU_tag(fpu.regvalid[adj]),
                      fpu.regs_80[adj].v, fpu.regs_80[adj].raw.h, (unsigned long long)fpu.regs_80[adj].raw.l);
#else
        DEBUG_ShowMsg(" st(%u): %s use80=%u val=%.16g (0x%016llx)", i, FPU_tag(fpu.regvalid[adj]),
                       fpu.use80[adj], fpu.regs[adj].v, (unsigned long long)fpu.regs[adj].raw);
#endif
    }

    DEBUG_EndPagedContent();
}

static void LogCPUInfo(void) {
	char out1[512];

    DEBUG_BeginPagedContent();

	sprintf(out1,"cr0:%08lX cr2:%08lX cr3:%08lX  cpl=%lx",
		(unsigned long)cpu.cr0,
		(unsigned long)paging.cr2,
		(unsigned long)paging.cr3,
		(unsigned long)cpu.cpl);
    DEBUG_ShowMsg("%s",out1);
	sprintf(out1,"eflags:%08lX [vm=%x iopl=%x nt=%x]",
		(unsigned long)reg_flags,
		(int)(GETFLAG(VM)>>17),
		(int)(GETFLAG(IOPL)>>12),
		(int)(GETFLAG(NT)>>14));
    DEBUG_ShowMsg("%s",out1);
	sprintf(out1,"GDT base=%08lX limit=%08lX",
		(unsigned long)cpu.gdt.GetBase(),
		(unsigned long)cpu.gdt.GetLimit());
    DEBUG_ShowMsg("%s",out1);
	sprintf(out1,"IDT base=%08lX limit=%08lX",
		(unsigned long)cpu.idt.GetBase(),
		(unsigned long)cpu.idt.GetLimit());
    DEBUG_ShowMsg("%s",out1);

	Bitu sel=CPU_STR();
	Descriptor desc;
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"TR selector=%04X, base=%08lX limit=%08lX*%X",(int)sel,(unsigned long)desc.GetBase(),(unsigned long)desc.GetLimit(),desc.saved.seg.g?0x4000:1);
        DEBUG_ShowMsg("%s",out1);
	}
	sel=CPU_SLDT();
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"LDT selector=%04X, base=%08lX limit=%08lX*%X",(int)sel,(unsigned long)desc.GetBase(),(unsigned long)desc.GetLimit(),desc.saved.seg.g?0x4000:1);
        DEBUG_ShowMsg("%s",out1);
	}

    DEBUG_EndPagedContent();
}

#if C_HEAVY_DEBUG
static void LogInstruction(uint16_t segValue, uint32_t eipValue,  ofstream& out) {
	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	if (cpuLogType == 3) { //Log only cs:ip.
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << endl;
		return;
	}

	PhysPt start = (PhysPt)GetAddress(segValue,eipValue);
	char dline[200];Bitu size;
	size = DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (showExtend && (cpuLogType > 0)) {
		res = AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
		if (reslen < 22) {
			memset(res + reslen, ' ', 22 - reslen);
			res[22] = 0;
		}
	}
	Bitu len = strlen(dline);
	if (len < 30) {
		memset(dline + len, ' ', 30 - len);
		dline[30] = 0;
	}

	// Get register values

	if(cpuLogType == 0) {
		out << setw(4) << SegValue(cs) << ":" << setw(4) << reg_eip << "  " << dline;
	} else if (cpuLogType == 1) {
		out << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res;
	} else if (cpuLogType == 2) {
		char ibytes[200]="";
		char tstamp[128];
		char tmpc[200];
		for (Bitu i=0; i<size; i++) {
			uint8_t value;
			if (mem_readb_checked((PhysPt)(start+i),&value)) sprintf(tmpc,"%s","?? ");
			else sprintf(tmpc,"%02X ",value);
			strcat(ibytes,tmpc);
		}
		len = strlen(ibytes);
		if (len < 21) {
			for (Bitu i = 0; i < 21 - len; i++) ibytes[len + i] = ' ';
			ibytes[21] = 0;
		}
		sprintf(tstamp,"%.6f",(double)PIC_FullIndex());
		out << tstamp << " " << setw(4) << SegValue(cs) << ":" << setw(8) << reg_eip << "  " << dline << "  " << res << "  " << ibytes;
	}

	out << " EAX:" << setw(8) << reg_eax << " EBX:" << setw(8) << reg_ebx
		<< " ECX:" << setw(8) << reg_ecx << " EDX:" << setw(8) << reg_edx
		<< " ESI:" << setw(8) << reg_esi << " EDI:" << setw(8) << reg_edi
		<< " EBP:" << setw(8) << reg_ebp << " ESP:" << setw(8) << reg_esp
		<< " DS:"  << setw(4) << SegValue(ds)<< " ES:"  << setw(4) << SegValue(es);

	if(cpuLogType == 0) {
		out << " SS:"  << setw(4) << SegValue(ss) << " C"  << (get_CF()>0)  << " Z"   << (get_ZF()>0)
			<< " S" << (get_SF()>0) << " O"  << (get_OF()>0) << " I"  << GETFLAGBOOL(IF);
	} else {
		out << " FS:"  << setw(4) << SegValue(fs) << " GS:"  << setw(4) << SegValue(gs)
			<< " SS:"  << setw(4) << SegValue(ss)
			<< " CF:"  << (get_CF()>0)  << " ZF:"   << (get_ZF()>0)  << " SF:"  << (get_SF()>0)
			<< " OF:"  << (get_OF()>0)  << " AF:"   << (get_AF()>0)  << " PF:"  << (get_PF()>0)
			<< " IF:"  << GETFLAGBOOL(IF);
	}
	if(cpuLogType == 2) {
		out << " TF:" << GETFLAGBOOL(TF) << " VM:" << GETFLAGBOOL(VM) <<" FLG:" << setw(8) << reg_flags
			<< " CR0:" << setw(8) << cpu.cr0;
	}
	out << endl;
}
#endif

#if 0
// DEBUG.COM stuff

class DEBUG : public Program {
public:
	DEBUG()		{ pDebugcom	= this;	active = false; };
	~DEBUG()	{ pDebugcom	= 0; };

	bool IsActive() { return active; };

	void Run(void)
	{
		if(cmd->FindExist("/NOMOUSE",false)) {
	        	real_writed(0,0x33<<2,0);
			return;
		}

		uint16_t commandNr = 1;
		if (!cmd->FindCommand(commandNr++,temp_line)) return;
		// Get filename
		char filename[128];
		safe_strncpy(filename,temp_line.c_str(),128);
		// Setup commandline
		char args[256+1];
		args[0]	= 0;
		bool found = cmd->FindCommand(commandNr++,temp_line);
		while (found) {
			if (strlen(args)+temp_line.length()+1>256) break;
			strcat(args,temp_line.c_str());
			found = cmd->FindCommand(commandNr++,temp_line);
			if (found) strcat(args," ");
		}
		// Start new shell and execute prog
		active = true;
		// Save cpu state....
		uint16_t oldcs	= SegValue(cs);
		uint32_t oldeip	= reg_eip;
		uint16_t oldss	= SegValue(ss);
		uint32_t oldesp	= reg_esp;

		// Start shell
		DOS_Shell shell;
		shell.Execute(filename,args);

		// set old reg values
		SegSet16(ss,oldss);
		reg_esp = oldesp;
		SegSet16(cs,oldcs);
		reg_eip = oldeip;
	};

private:
	bool	active;
};
#endif

void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off)
{
#if !defined(OSFREE)
# if C_DEBUG
    if (debugger_break_on_exec) {
        if (agent_target_launch && !agent_target_psp) {
            agent_target_psp=dos.psp();
            agent_target_owner=DOSBOX_MachineServiceOwner().token;
        }
#  if defined(C_DOSBOX_AGENT)
        // The new entry breakpoint is created at the current CS:IP. The
		// existing bulk activation intentionally skips that address, so arm
		// this one explicitly before preserving the other breakpoint state.
		CBreakpoint* const entry_breakpoint = CBreakpoint::AddBreakpoint(seg,off,true);
		entry_breakpoint->Activate(true);
        CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		agent_entry_breakpoint_sequence.fetch_add(1, std::memory_order_relaxed);
#  else
        CBreakpoint::AddBreakpoint(seg, off, true);
        CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs) + reg_eip);
#  endif
        debugger_break_on_exec = false;
    }
# endif
#endif
#if 0
	if (pDebugcom && pDebugcom->IsActive()) {
		CBreakpoint::AddBreakpoint(seg,off,true);
		CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
		pDebugcom = 0;
	};
#endif
}

#if C_DEBUG_LUA
static bool Debugger_ExecutionStop();
static void ExecutionFinish(const char *reason);
#endif
Bitu DEBUG_EnableDebugger(void)
{
#if C_DEBUG_LUA
    if(local_driver_started && local_driver_thread.joinable()) {
        local_driver_host_break=true;
        CPU_CycleLeft+=CPU_Cycles;CPU_Cycles=0;
        DEBUG_EnterCommonStoppedState();
        CBreakpoint::DeactivateBreakpoints();
        exitLoop=true;
        DOSBOX_SetLoop(DEBUG_Loop);
        return 0;
    }
    if(predicate_selected) {
        predicate_selected=false;
        predicate_owner=DOSBOX_MachineServiceOwner();
        const auto denial=DOSBOX_MachineServiceAdmission();
        predicate_denial=denial;predicate_registers=Debugger_GetRegisters();
        predicate_hold_start=Debugger_LiveHostNow();predicate_hold_reported=false;
        predicate_hold=denial==MachineService::Denial::None ? PredicateHold::Pending : PredicateHold::Unsupported;
        CPU_CycleLeft+=CPU_Cycles;CPU_Cycles=0;
        DEBUG_EnterCommonStoppedState();CBreakpoint::DeactivateBreakpoints();
        exitLoop=true;DOSBOX_SetLoop(DEBUG_Loop);
        if(predicate_hold==PredicateHold::Pending)DOSBOX_RequestMachineService();
        else {
            snprintf(predicate_summary.last.diagnostic,sizeof(predicate_summary.last.diagnostic),"hook-unsupported-context");
            PredicateReceipt("unsupported",true);
            fprintf(stderr,"hook-unsupported-context id=%llu cs=%04x ip=%08x owner=%u depth=%u denial=%u\n",
            (unsigned long long)predicate_selected_id,SegValue(cs),reg_eip,unsigned(predicate_owner.kind),predicate_owner.depth,unsigned(predicate_denial));
        }
        return 0;
    }
    // This callback publishes a stop only; Lua runs later at handler return.
    if (Debugger_ExecutionStop()) return 0;
    if(Debugger_LiveGenuineStop())return 0;
#endif
    // Active execution uses a native held-stop transition, not the UI toggle
    // (which may require a tty or apply a run-on-entry preference).
    if (DEBUG_HoldNativeExecution()) return 0;
	exitLoop = true;

	if (!debugging || (debugging && debug_running))
		DEBUG_Enable_Handler(true);

	CPU_CycleLeft += CPU_Cycles;
	CPU_Cycles = 0;
	return 0;
}

#if C_DEBUG_LUA
static uint64_t live_stop_token=0,live_stop_started=0,live_stop_finished=0;
static bool live_error=false;
static uint64_t Debugger_LiveHostNow() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
uint64_t Debugger_LiveStopStarted() {return live_stop_started;}
uint64_t Debugger_LiveStopFinished() {return live_stop_finished;}
const char *Debugger_LiveStopReason() {
    if(live_error)return "error";
    if(!Debugger_LiveEffectiveStopped())return "running";
    return live_stop_token && DOSBOX_MachineServiceOwner().token==live_stop_token ? "deliberate":"genuine";
}
static bool live_slice=false,live_decided=false;
static bool live_saved_active=false,live_saved_watch=false,live_saved_normal=false;
static Bitu Debugger_LiveIdle() {
    // No PIC IRQs, CPU or ncurses while deliberately stopped.
    Debugger_EntryMaintain();
    GFX_Events();Debugger_LuaLivePump();SDL_Delay(1);return 0;
}
bool Debugger_LiveEffectiveStopped() {
    return live_slice && !live_decided ?
        live_saved_active && !live_saved_watch && !live_saved_normal : Debugger_GetStatus().stopped;
}
void Debugger_LiveBeginSlice() {
    assert(!live_slice);
    live_saved_active=debugging;live_saved_watch=debug_running;live_saved_normal=runnormal;
    live_slice=true;live_decided=false;DEBUG_EnterCommonStoppedState();
}
void Debugger_LiveEndSlice() {
    assert(live_slice);
    if(!live_decided) {debugging=live_saved_active;debug_running=live_saved_watch;runnormal=live_saved_normal;}
    live_slice=false;
}
bool Debugger_LiveStop(uint64_t token) {
    const auto owner=DOSBOX_MachineServiceOwner();
    if(!token || owner.token!=token)return false;
    const bool provisional_stop=Debugger_PredicateHeld();
    if(provisional_stop) {
        if(predicate_owner.token!=token || predicate_hold==PredicateHold::Unsupported)return false;
        // Explicit outer stop wins over the provisional stopped projection.
        PredicateCount(predicate_summary.propagated);PredicateReceipt("bypassed",true);
        predicate_hold=PredicateHold::None;
    } else if(Debugger_LiveEffectiveStopped())return live_stop_token==token;
    live_stop_token=token;live_stop_started=Debugger_LiveHostNow();live_stop_finished=0;
    ExecutionFinish("interrupted");
    live_decided=true;DEBUG_EnterCommonStoppedState();
    CBreakpoint::DeactivateBreakpoints();DOSBOX_SetLoop(Debugger_LiveIdle);
    if(provisional_stop) {GFX_FaultLuaTasks();GFX_PumpLuaTasks(false,true);}
    return true;
}
bool Debugger_LocalClaimStop(NativeDebug::Attachment attachment,
                            NativeDebug::Context context,
                            const NativeFrontier::Descriptor &seam) {
    const auto owner=DOSBOX_MachineServiceOwner();
    if(attachment.serial!=local_driver_attachment.serial ||
       !native_execution.canEnqueue() || !NativeFrontier::frontiers().ownsHold(seam) ||
       seam.kind!=NativeFrontier::Kind::LoopStop || seam.decoder_entered ||
       seam.callback_entered || owner.kind!=MachineService::Kind::ShellExecGuest ||
       owner.depth!=2 || !DEBUG_AdmitNativeState(attachment,context) ||
       Debugger_PredicateHeld() || live_error)return false;
    // Claim ownership only: preserve the actual hit and stopped machine image.
    live_stop_token=owner.token;live_stop_started=Debugger_LiveHostNow();
    live_stop_finished=0;
    return true;
}
bool Debugger_LiveCheckpointStop(uint64_t token) {
    if (!Debugger_LiveOwnsStop(token)) return false;
    live_decided=true;
    DEBUG_EnterCommonStoppedState();
    CBreakpoint::DeactivateBreakpoints();
    DOSBOX_SetLoop(Debugger_LiveIdle);
    return true;
}
bool Debugger_LiveGenuineStop() {
    const auto owner=DOSBOX_MachineServiceOwner();
    if(!Debugger_LuaLiveConfigured() || !owner.token ||
       (owner.kind!=MachineService::Kind::RootGuest && owner.kind!=MachineService::Kind::ShellExecGuest))return false;
    if(DOSBOX_GetLoop()==Debugger_LiveIdle)return true;
    ExecutionFinish("interrupted");
    live_stop_token=0;live_stop_started=Debugger_LiveHostNow();live_stop_finished=0;
    CPU_CycleLeft+=CPU_Cycles;CPU_Cycles=0;
    DEBUG_EnterCommonStoppedState();CBreakpoint::DeactivateBreakpoints();
    DOSBOX_SetLoop(Debugger_LiveIdle);return true;
}
void Debugger_LiveError() {
    ExecutionFinish("error");
    live_error=true;live_decided=true;
    if(!live_stop_started || live_stop_finished)live_stop_started=Debugger_LiveHostNow();
    live_stop_finished=0;live_stop_token=0;
    DEBUG_EnterCommonStoppedState();CBreakpoint::DeactivateBreakpoints();
    DOSBOX_SetLoop(Debugger_LiveIdle);
}
void Debugger_LivePhaseCompleted(const DebuggerStopEvent &event) {
    if (event.type == DebuggerStopType::Error ||
        event.type == DebuggerStopType::Breakpoint ||
        event.type == DebuggerStopType::ManualStop) Debugger_LiveError();
}
bool Debugger_LiveOwnsStop(uint64_t token) {
    return !live_error && token && live_stop_token == token &&
        DOSBOX_MachineServiceOwner().token == token && Debugger_LiveEffectiveStopped();
}
// One machine-owner target; no guest loop or Lua call in the CPU hook.
static DebuggerExecutionTarget execution_target;
static bool execution_match = false;
const DebuggerExecutionTarget &Debugger_ExecutionTarget() { return execution_target; }
static bool ExecutionArmed() { return !strcmp(execution_target.reason,"armed"); }
static void ExecutionFinish(const char *reason) {
 if (!ExecutionArmed()) return;
 bool cleared=false;
 Debugger_ClearInternalTemporaryBreakpoint(execution_target.breakpoint_id,&cleared);
 LOG_MSG("Execution target terminal generation=%llu id=%llu reason=%s cleared=%u",
         (unsigned long long)execution_target.generation,
         (unsigned long long)execution_target.breakpoint_id,reason,unsigned(cleared));
 // Freeze at this existing transition, never on a later diagnostic read.
 // On owner loss these are the *current* owner's hook registers, not the
 // departed guest's return site. Fault/unsupported decoder state is unknown.
 const auto terminal_owner=DOSBOX_MachineServiceOwner();
 execution_target.terminal_owner=terminal_owner.token;
 execution_target.terminal_owner_kind=unsigned(terminal_owner.kind);
 execution_target.terminal_owner_depth=terminal_owner.depth;
 execution_target.terminal_flags_valid=!strcmp(reason,"hit");
 execution_target.terminal_regs_valid=execution_target.terminal_flags_valid ||
     (cpudecoder==CPU_Core_Normal_Run && strcmp(reason,"error"));
 if (execution_target.terminal_regs_valid)
  execution_target.terminal_regs=execution_target.terminal_flags_valid ?
      execution_target.regs : Debugger_GetRegisters();
 execution_target.reason=reason;
 execution_target.end_ms=Debugger_LiveHostNow();
 execution_target.tick=PIC_Ticks;
}
void Debugger_PredicateResetForRestore() {
    assert(!Debugger_PredicateHeld());
    CBreakpoint::DeleteOwned(DebuggerBreakpointOwner::Lua);
    ExecutionFinish("cancelled");execution_match=false;
    predicate_selected=false;predicate_hold=PredicateHold::None;
    predicate_summary.terminal_valid=false;
    predicate_summary.last=DebuggerPredicateReceipt{};predicate_summary.last_valid=false;
}
void Debugger_ExecutionOwnerDeparting(uint64_t token) {
 // Scope still exists here, including during exception unwinding. Only retire
 // its exact target; no service, guest execution, or continuation restoration.
 Debugger_StateOwnerDeparting(token);
 if (ExecutionArmed() && token && token==execution_target.owner)
  ExecutionFinish("owner_lost");
}
bool Debugger_ExecutionBreakpointEligible(uint64_t id) {
#if C_DEBUG_LUA
 if(std::find(native_condition_breakpoints.begin(),native_condition_breakpoints.end(),id)!=native_condition_breakpoints.end() &&
    native_execution.activeRequest() && DOSBOX_MachineServiceRelation(native_execution.activeRequest()->expected.invocation)!=MachineService::Relation::Current)return false;
 if(local_driver_target && id==native_execution_breakpoint &&
    native_execution.activeRequest() &&
    native_execution.activeRequest()->frontend==NativeDebug::Frontend::Lua &&
    DOSBOX_MachineServiceRelation(local_driver_target_owner)!=MachineService::Relation::Current)
     return false;
#endif
 return id!=execution_target.breakpoint_id || !ExecutionArmed() ||
     DOSBOX_MachineServiceRelation(execution_target.owner)==MachineService::Relation::Current;
}
void Debugger_ExecutionMaintain(bool channel_closed) {
 if (!ExecutionArmed()) return;
 if (channel_closed) ExecutionFinish("channel_closed");
 else if (DOSBOX_MachineServiceRelation(execution_target.owner)==MachineService::Relation::Indeterminate) ExecutionFinish("owner_indeterminate");
 else if (DOSBOX_MachineServiceRelation(execution_target.owner)==MachineService::Relation::Absent) ExecutionFinish("owner_lost");
 else if (cpudecoder != CPU_Core_Normal_Run) ExecutionFinish("unsupported-core");
 else if (Debugger_LiveHostNow() >= execution_target.deadline_ms) ExecutionFinish("nonarrival");
}
const char *Debugger_ExecutionArm(uint16_t cs_value,uint16_t ip_value,uint32_t wall_ms) {
 Debugger_ExecutionMaintain();
#if !C_HEAVY_DEBUG
 return "unsupported-build";
#else
 if (cpudecoder != CPU_Core_Normal_Run) return "unsupported-core";
 const auto owner=DOSBOX_MachineServiceOwner();
 if (!owner.token || !Debugger_LuaLiveConfigured()) return "owner-unavailable";
 // Arm is data-only: a clean stop must belong to this machine owner.
 // Keep genuine/foreign/error stops non-continuable; never resume here.
 if (live_error || (Debugger_LiveEffectiveStopped() && !Debugger_LiveOwnsStop(owner.token)))
  return "requires-running-or-owned-stop";
 if (ExecutionArmed()) return "already-armed";
 if (!wall_ms || wall_ms>10000) return "invalid-duration";
 if (execution_target.generation>=uint64_t(INT64_MAX)) return "generation-exhausted";
 if (SegValue(cs)==cs_value && reg_eip==ip_value) return "already-at-target";
 const auto address=Debugger_ResolveAddress(cs_value,ip_value);
 uint64_t id=0;bool created=false;
 const auto result=Debugger_PrepareRunUntilAddress(address,&id,&created,true);
 if (result!=DebuggerTempBreakpointResult::Created) return "target-collision-or-invalid";
 const auto generation=execution_target.generation+1;
 execution_target={};execution_match=false;
 execution_target.generation=generation;execution_target.owner=owner.token;
 execution_target.arm_owner_kind=unsigned(owner.kind);execution_target.arm_owner_depth=owner.depth;
 execution_target.breakpoint_id=id;execution_target.cs=cs_value;execution_target.ip=ip_value;
 execution_target.start_ms=Debugger_LiveHostNow();execution_target.deadline_ms=execution_target.start_ms+wall_ms;
 execution_target.reason="armed";
 CBreakpoint::FindPhysBreakpoint(cs_value,ip_value,true)->Activate(true);
 return nullptr;
#endif
}
const char *Debugger_ExecutionCancel(uint64_t generation) {
 if (!generation || generation!=execution_target.generation ||
     execution_target.owner!=DOSBOX_MachineServiceOwner().token) return "stale-generation-or-owner";
 ExecutionFinish("canceled");return nullptr;
}
void Debugger_ExecutionRecord(uint64_t id,uint64_t hits) {
 if (!ExecutionArmed()) return;
 if (id==execution_target.breakpoint_id && Debugger_ExecutionBreakpointEligible(id)) {
  execution_match=true;execution_target.hit_count=hits;
 }
}
static bool Debugger_ExecutionStop() {
 if (!ExecutionArmed()) return false;
 if (!execution_match) {ExecutionFinish("interrupted");return false;}
 if (Debugger_LiveHostNow() >= execution_target.deadline_ms) {
  ExecutionFinish("nonarrival");execution_match=false;return false;
 }
 // Normal core has materialized flags before invoking debugCallback.
 execution_target.regs=Debugger_GetRegisters();
 ExecutionFinish("hit");execution_match=false;
 live_stop_token=execution_target.owner;live_stop_started=execution_target.end_ms;
 live_stop_finished=0;
 CPU_CycleLeft+=CPU_Cycles;CPU_Cycles=0;
 DEBUG_EnterCommonStoppedState();CBreakpoint::DeactivateBreakpoints();
 DOSBOX_SetLoop(Debugger_LiveIdle);return true;
}

bool Debugger_LiveResume(uint64_t token) {
    if (!NativeControl::rawEffectAllowed()) return false;
    if(DEBUG_ContinuationUnavailable() || live_error || !token || DOSBOX_MachineServiceOwner().token!=token)return false;
    if(!Debugger_LiveEffectiveStopped())return true;
    // A genuine debugger stop is not our deliberate stop to undo.
    if(live_stop_token!=token)return false;
    predicate_summary.terminal_valid=false;
    live_stop_finished=Debugger_LiveHostNow();live_stop_token=0;live_decided=true;
    CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs)+reg_eip);
    debugging=false;debug_running=false;runnormal=false;exitLoop=false;
    DOSBOX_SetNormalLoop();return true;
}
#endif

#if C_DEBUG_LUA
static void DEBUG_EnableStartupLuaStoppedState(void)
{
	/* Use the same debugger-common stopped-state transition as normal debugger
	 * activation, but intentionally do not call DEBUG_EnableDebugger() or
	 * DEBUG_Enable_Handler(): those own ncurses stdio and keep the existing
	 * isatty(0/1/2) gate for interactive debugger startup. This helper is called
	 * only from the explicit C_DEBUG_LUA -debug-lua-script path before
	 * DOSBOX_RunMachine(), and Lua still runs only after Debugger_GetStatus()
	 * reports stopped.
	 */
	exitLoop = true;
	CPU_CycleLeft += CPU_Cycles;
	CPU_Cycles = 0;
	DEBUG_EnterCommonStoppedState();
}

bool DEBUG_RunStartupLuaScriptInStoppedState(const char *script_path)
{
	if (script_path == NULL || script_path[0] == '\0')
		return false;

	DEBUG_EnableStartupLuaStoppedState();

	const DebuggerStatus status = Debugger_GetStatus();
	if (!status.stopped) {
		LOG_MSG("Lua debugger: startup script '%s' was requested, but the debugger did not enter a stopped state", script_path);
		return false;
	}

	return Debugger_LuaRunScriptInStoppedState(script_path);
}
#endif

// INIT

void DBGBlock::set_data_view(unsigned int view) {
    void DrawBars(void);

    if (data_view != view) {
        data_view  = view;

        if (win_data) wclear(win_data);

        switch (view) {
            case DATV_SEGMENTED:
                win_title[DBGBlock::WINI_DATA] = "Data view (segmented)";
                break;
            case DATV_VIRTUAL:
                win_title[DBGBlock::WINI_DATA] = "Data view (virtual)";
                break;
            case DATV_PHYSICAL:
                win_title[DBGBlock::WINI_DATA] = "Data view (physical)";
                break;
        }

        DrawBars();
    }
}

void DEBUG_SetupConsole(void) {
	if (dbg.win_main == NULL) {
        LOG(LOG_MISC, LOG_DEBUG)("DEBUG_SetupConsole initializing GUI");

        dbg.set_data_view(DBGBlock::DATV_SEGMENTED);

#ifdef WIN32
		WIN32_Console();
#else
		tcgetattr(0,&consolesettings);
#endif	
		//	dbg.active_win=3;
		/* Start the Debug Gui */
		DBGUI_StartUp();
	}
}

void DEBUG_ShutDown(Section * /*sec*/) {
#if C_DEBUG_LUA
    Debugger_LuaInteractiveReset();
#endif
	TIMER_DelTickHandler(ControlServer_Poll);
	ControlServer_Stop();

	CBreakpoint::DeleteAll();
	CDebugVar::DeleteAll();
	if (dbg.win_main != NULL) {
        LOG(LOG_MISC, LOG_DEBUG)("DEBUG_Shutdown freeing ncurses state");
		curs_set(old_cursor_state);

        void DEBUG_GUI_DestroySubWindows(void);
        DEBUG_GUI_DestroySubWindows();

//      if (dbg.win_main) delwin(dbg.win_main);
		dbg.win_main = NULL;

        endwin();

#ifndef WIN32
		tcsetattr(0,TCSANOW,&consolesettings);
#endif
	}
}

Bitu debugCallback;

bool DEBUG_IsNativeDebuggerCallback(Bits callback) {
    return callback>0 && (Bitu)callback==debugCallback;
}

void DEBUG_ReinitCallback(void) {
    /* this is REQUIRED after loading a custom BIOS */
	debugCallback=CALLBACK_Allocate();
	CALLBACK_Setup(debugCallback,DEBUG_EnableDebugger,CB_RETF,"debugger");
    LOG_MSG("Debugger callback entry %04x:%04x",RealSeg(CALLBACK_RealPointer(debugCallback)),RealOff(CALLBACK_RealPointer(debugCallback)));
}

void DEBUG_Init() {
    // One hook dispatches to the existing worker owner or the built-in slot.
    if (!NativeFrontier::frontiers().bind([](void *opaque,const NativeFrontier::Descriptor &seam) {
        DEBUG_NativeConditionFrontier(opaque,seam);
#if C_DEBUG_LUA
        if (control->lua_driver.script!=LuaDriver::Options::Script::None) LocalDriverFrontier(opaque,seam);
#endif
    },nullptr)) E_Exit("native debugger frontier already bound");
    LOG(LOG_MISC, LOG_DEBUG)("Initializing debug system");

	Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
	const int mcp_server_port = section != NULL ? section->Get_int("mcp_server") : 0;
	if (mcp_server_port > 0) {
		ControlServer_Start(static_cast<uint16_t>(mcp_server_port));

	}

	/* Reset code overview and input line */
	memset((void*)&codeViewData,0,sizeof(codeViewData));
	/* Setup callback */
	debugCallback=CALLBACK_Allocate();
	CALLBACK_Setup(debugCallback,DEBUG_EnableDebugger,CB_RETF,"debugger");
    LOG_MSG("Debugger callback entry %04x:%04x",RealSeg(CALLBACK_RealPointer(debugCallback)),RealOff(CALLBACK_RealPointer(debugCallback)));

	/* shutdown function */
	AddExitFunction(AddExitFunctionFuncPair(DEBUG_ShutDown));
}

// DEBUGGING VAR STUFF

void CDebugVar::InsertVariable(char* name, PhysPt adr)
{
	varList.push_back(new CDebugVar(name,adr));
}

void CDebugVar::DeleteAll(void)
{
	std::vector<CDebugVar*>::iterator i;
	for(i=varList.begin(); i != varList.end(); ++i) {
		CDebugVar* bp = *i;
		delete bp;
	}
	(varList.clear)();
}

CDebugVar* CDebugVar::FindVar(PhysPt pt)
{
	if (varList.empty()) return nullptr;

	std::vector<CDebugVar*>::size_type s = varList.size();
	for(std::vector<CDebugVar*>::size_type i = 0; i != s; i++) {
		CDebugVar* bp = varList[i];
		if (bp->GetAdr() == pt) return bp;
	}
	return nullptr;
}

CDebugVar* CDebugVar::FindVar(const std::string& name)
{
	for (auto* variable : varList) {
		if (strcasecmp(name.c_str(), variable->GetName()) == 0) return variable;
	}
	return nullptr;
}

bool CDebugVar::SaveVars(char* name) {
	if (varList.size() > 65535) return false;

	FILE* f = fopen(name,"wb+");
	if (!f) return false;

	// write number of vars
	uint16_t num = (uint16_t)varList.size();
	fwrite(&num,1,sizeof(num),f);

	std::vector<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); ++i) {
		bp = *i;
		// name
		fwrite(bp->GetName(),1,16,f);
		// adr
		PhysPt adr = bp->GetAdr();
		fwrite(&adr,1,sizeof(adr),f);
	}
	fclose(f);
	return true;
}

bool CDebugVar::LoadVars(char* name)
{
	FILE* f = fopen(name,"rb");
	if (!f) return false;

	// read number of vars
	uint16_t num;
	if (fread(&num,sizeof(num),1,f) != 1) {
		fclose(f);
		return false;
	}

	for (uint16_t i=0; i<num; i++) {
		char name2[16];
		// name
		if (fread(name2,16,1,f) != 1) break;
		// adr
		PhysPt adr;
		if (fread(&adr,sizeof(adr),1,f) != 1) break;
		// insert
		InsertVariable(name2,adr);
	}
	fclose(f);
	return true;
}

static void SaveMemory(const char *filename, uint16_t seg, uint32_t ofs1, uint32_t num) {
	auto address = GetAddress(seg, ofs1);
	std::ofstream file(filename);
	if (!file.is_open()) {
		DEBUG_ShowMsg("DEBUG: Unable to open %s for writing.\n", filename);
		return;
	}

	while (num > 0) {
		auto row_size = num < 16 ? num : 16;
		std::ostringstream row;
		row << std::uppercase << std::hex << std::setfill('0')
		    << std::setw(4) << seg << ':' << std::setw(4) << ofs1 << "   ";
		for (uint32_t x = 0; x < row_size; ++x) {
			uint8_t value = 0;
			if (mem_readb_checked(address++,&value)) {
				row << "?? ";
			} else {
				row << std::setw(2) << static_cast<unsigned int>(value) << ' ';
			}
		}
		file << row.str() << '\n';
		if (!file) {
			DEBUG_ShowMsg("DEBUG: Memory dump failed.\n");
			return;
		}
		ofs1 += row_size;
		num -= row_size;
	}
	file.close();
	DEBUG_ShowMsg("DEBUG: Memory dump success.\n");
}

static void SaveMemoryBin(const char *filename, uint16_t seg, uint32_t ofs1, uint32_t num) {
	auto address = GetAddress(seg, ofs1);
	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open()) {
		DEBUG_ShowMsg("DEBUG: Unable to open %s for writing.\n", filename);
		return;
	}

	for (uint32_t x = 0; x < num; ++x) {
		uint8_t value = 0;
		mem_readb_checked(address++, &value);
		file.write((const char*)&value, sizeof(value));
		if (!file) {
			DEBUG_ShowMsg("DEBUG: Memory binary dump failed.\n");
			return;
		}
	}

	file.close();
	DEBUG_ShowMsg("DEBUG: Memory dump binary success.\n");
}

static void OutputVecTable(char* filename) {
	FILE* f = fopen(filename, "wt");
	if (!f)
	{
		DEBUG_ShowMsg("DEBUG: Output of interrupt vector table failed.\n");
		return;
	}

    for(unsigned int i = 0; i < 256; i++) {
        if(cpu.pmode) {
            Descriptor gate;
            if(cpu.idt.GetDescriptor((Bitu)i << 3u, gate)) {
                fprintf(f, "INT %02X:  %04X:%04X\n", i, (int)gate.GetSelector(), (int)gate.GetOffset());
            }
            else {
                DEBUG_ShowMsg("INTVEC unable to retrieve vector");
            }
        }
        else {
            fprintf(f, "INT %02X:  %04X:%04X\n", i, mem_readw(i * 4u + 2u), mem_readw(i * 4u));
        }
    }

	fclose(f);
	DEBUG_ShowMsg("DEBUG: Interrupt vector table written to %s.\n", filename);
}

#define DEBUG_VAR_BUF_LEN 16
static void DrawVariables(void) {
	if (CDebugVar::varList.empty()) return;

	char buffer[DEBUG_VAR_BUF_LEN];
	std::vector<CDebugVar*>::size_type s = CDebugVar::varList.size();
	bool windowchanges = false;

	for(std::vector<CDebugVar*>::size_type i = 0; i != s; i++) {

		if (i == 4*3) {
			/* too many variables */
			break;
		}

		CDebugVar *dv = CDebugVar::varList[i];
		uint16_t value;
		bool varchanges = false;
		bool has_no_value = mem_readw_checked(dv->GetAdr(),&value);
		if (has_no_value) {
			snprintf(buffer,DEBUG_VAR_BUF_LEN, "%s", "??????");
			dv->SetValue(false,0);
			varchanges = true;
		} else {
			if ( dv->HasValue() && dv->GetValue() == value) {
				//It already had a value and it didn't change (most likely case)
			} else {
				dv->SetValue(true,value);
				snprintf(buffer,DEBUG_VAR_BUF_LEN, "0x%04x", value);
				varchanges = true;
			}
		}

		if (varchanges) {
			unsigned int y = (unsigned int)(i / 3u);
			unsigned int x = (i % 3u) * 26u;
			mvwprintw(dbg.win_var, (int)y,  (int)x, "%s", dv->GetName());
			mvwprintw(dbg.win_var, (int)y, ((int)x + DEBUG_VAR_BUF_LEN + 1), "%s", buffer);
			windowchanges = true; //Something has changed in this window
		}
	}

	if (windowchanges) wrefresh(dbg.win_var);
}
#undef DEBUG_VAR_BUF_LEN
// HEAVY DEBUGGING STUFF

#if C_HEAVY_DEBUG

const uint32_t LOGCPUMAX = 20000;

static uint32_t logCount = 0;

struct TLogInst {
	uint16_t s_cs;
	uint32_t eip;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t esp;
	uint16_t s_ds;
	uint16_t s_es;
	uint16_t s_fs;
	uint16_t s_gs;
	uint16_t s_ss;
	bool c;
	bool z;
	bool s;
	bool o;
	bool a;
	bool p;
	bool i;
	uint32_t flags;
	char dline[31];
	char res[23];
};

TLogInst logInst[LOGCPUMAX];

#if defined(C_DOSBOX_AGENT)
static bool agent_trace_active = false;
static uint64_t agent_trace_owner = 0;
static NativeDebug::Attachment agent_trace_attachment;
static uint32_t agent_trace_remaining = 0;
#endif
static vector<DEBUG_AgentTraceEvent> agent_trace_events;

// Persistent heavy instrumentation belongs to the sole attached session.
// Operation logging is already closed before terminal publication. Do not leave
// these flags or retained guest history armed across replacement attachments.
static void DEBUG_CleanupSessionInstrumentation() {
    logHeavy=false;
    zeroProtect=false;
    logCount=0;
    memset(logInst,0,sizeof(logInst));
#if defined(C_DOSBOX_AGENT)
    agent_trace_active=false;
    agent_trace_owner=0;
    agent_trace_attachment={};
    agent_trace_remaining=0;
    agent_trace_events.clear();
#endif
}

void DEBUG_HeavyLogInstruction(void) {

	static char empty[23] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	PhysPt start = (PhysPt)GetAddress(SegValue(cs),reg_eip);
	char dline[200];
	DasmI386(dline, start, reg_eip, cpu.code.big);
	char* res = empty;
	if (showExtend) {
		res = AnalyzeInstruction(dline,false);
		if (!res || !(*res)) res = empty;
		Bitu reslen = strlen(res);
        if (reslen < 22) {
            memset(res + reslen, ' ', 22 - reslen);
            res[22] = 0;
        }
	}

	Bitu len = strlen(dline);
	if (len < 30) memset(dline + len,' ',30 - len);
	dline[30] = 0;

	TLogInst & inst = logInst[logCount];
	strcpy(inst.dline,dline);
	inst.s_cs = SegValue(cs);
	inst.eip  = reg_eip;
	strcpy(inst.res,res);
	inst.eax  = reg_eax;
	inst.ebx  = reg_ebx;
	inst.ecx  = reg_ecx;
	inst.edx  = reg_edx;
	inst.esi  = reg_esi;
	inst.edi  = reg_edi;
	inst.ebp  = reg_ebp;
	inst.esp  = reg_esp;
	inst.s_ds = SegValue(ds);
	inst.s_es = SegValue(es);
	inst.s_fs = SegValue(fs);
	inst.s_gs = SegValue(gs);
	inst.s_ss = SegValue(ss);
	inst.c    = get_CF()>0;
	inst.z    = get_ZF()>0;
	inst.s    = get_SF()>0;
	inst.o    = get_OF()>0;
	inst.a    = get_AF()>0;
	inst.p    = get_PF()>0;
	inst.i    = GETFLAGBOOL(IF);
	inst.flags = static_cast<uint32_t>(reg_flags);

	if (++logCount >= LOGCPUMAX) logCount = 0;
}

#if defined(C_DEBUG) && defined(C_DOSBOX_AGENT)
bool DEBUG_AgentStartTrace(const uint32_t instruction_count, NativeDebug::Attachment attachment, NativeDebug::Context expected)
{
    if (!DEBUG_AdmitNativeObservation(attachment,expected) || !DEBUG_AdmitNativeEffect(attachment)) return false;
	if (instruction_count == 0 || agent_trace_active)
		return false;
	agent_trace_events.clear();
	agent_trace_events.reserve(instruction_count);
	agent_trace_remaining = instruction_count;
    agent_trace_owner = DOSBOX_MachineServiceOwner().token;
    agent_trace_attachment = attachment;
	agent_trace_active = true;
	return true;
}

// Retained samples and cancellation use the captured session resource, not a
// newly attached frontend or a stopped edit. This remains usable while running.
static bool DEBUG_AdmitAgentTrace(NativeDebug::Attachment attachment)
{
    return native_execution.admits(attachment) &&
        attachment.serial==agent_trace_attachment.serial &&
        NativeControl::diagnosticEffectAllowed();
}

bool DEBUG_AgentStopTrace(uint32_t* event_count, NativeDebug::Attachment attachment)
{
	if (event_count == nullptr)
		return false;
    *event_count = 0;
    // Stopping owned instrumentation is supported while the guest runs; it
    // needs session/resource authority, not a held edit or empty result slot.
    if (!DEBUG_AdmitAgentTrace(attachment)) return false;
	agent_trace_active = false;
	agent_trace_remaining = 0;
    agent_trace_owner = 0;
	*event_count = static_cast<uint32_t>(agent_trace_events.size());
	return true;
}

void DEBUG_AgentTraceOwnerDeparting(uint64_t token)
{
    if (!token || token!=agent_trace_owner) return;
    agent_trace_active=false;
    agent_trace_remaining=0;
    agent_trace_owner=0;
}

bool DEBUG_AgentTraceIsActive(void)
{
	return agent_trace_active;
}

bool DEBUG_AgentCopyTraceEvents(vector<DEBUG_AgentTraceEvent>* events, NativeDebug::Attachment attachment)
{
    if (!events || !DEBUG_AdmitAgentTrace(attachment)) return false;
    *events = agent_trace_events;
    return true;
}

static void DEBUG_AgentCaptureTraceEvent(void)
{
	DEBUG_HeavyLogInstruction();
	const uint32_t last_index = logCount == 0 ? LOGCPUMAX - 1 : logCount - 1;
	const TLogInst& inst = logInst[last_index];
	DEBUG_AgentTraceEvent event;
	event.cs = inst.s_cs;
	event.instruction_pointer = inst.eip;
	event.eax = inst.eax;
	event.ebx = inst.ebx;
	event.ecx = inst.ecx;
	event.edx = inst.edx;
	event.esi = inst.esi;
	event.edi = inst.edi;
	event.ebp = inst.ebp;
	event.esp = inst.esp;
	event.ds = inst.s_ds;
	event.es = inst.s_es;
	event.fs = inst.s_fs;
	event.gs = inst.s_gs;
	event.ss = inst.s_ss;
	event.flags = inst.flags;
    event.protected_mode = cpu.pmode;
	event.instruction = inst.dline;
	event.analysis = inst.res;
	agent_trace_events.push_back(event);
}
#endif

void DEBUG_HeavyWriteLogInstruction(void) {
	if (!logHeavy) return;
	logHeavy = false;

	DEBUG_ShowMsg("DEBUG: Creating cpu log LOGCPU_INT_CD.TXT\n");

	ofstream out("LOGCPU_INT_CD.TXT");
	if (!out.is_open()) {
		DEBUG_ShowMsg("DEBUG: Failed.\n");
		return;
	}
	out << hex << noshowbase << setfill('0') << uppercase;
	uint32_t startLog = logCount;
	do {
		// Write Instructions
		TLogInst & inst = logInst[startLog];
		out << setw(4) << inst.s_cs << ":" << setw(8) << inst.eip << "  "
		    << inst.dline << "  " << inst.res << " EAX:" << setw(8)<< inst.eax
		    << " EBX:" << setw(8) << inst.ebx << " ECX:" << setw(8) << inst.ecx
		    << " EDX:" << setw(8) << inst.edx << " ESI:" << setw(8) << inst.esi
		    << " EDI:" << setw(8) << inst.edi << " EBP:" << setw(8) << inst.ebp
		    << " ESP:" << setw(8) << inst.esp << " DS:"  << setw(4) << inst.s_ds
		    << " ES:"  << setw(4) << inst.s_es<< " FS:"  << setw(4) << inst.s_fs
		    << " GS:"  << setw(4) << inst.s_gs<< " SS:"  << setw(4) << inst.s_ss
		    << " CF:"  << inst.c  << " ZF:"   << inst.z  << " SF:"  << inst.s
		    << " OF:"  << inst.o  << " AF:"   << inst.a  << " PF:"  << inst.p
		    << " IF:"  << inst.i  << endl;

/*		fprintf(f,"%04X:%08X   %s  %s  EAX:%08X EBX:%08X ECX:%08X EDX:%08X ESI:%08X EDI:%08X EBP:%08X ESP:%08X DS:%04X ES:%04X FS:%04X GS:%04X SS:%04X CF:%01X ZF:%01X SF:%01X OF:%01X AF:%01X PF:%01X IF:%01X\n",
			logInst[startLog].s_cs,logInst[startLog].eip,logInst[startLog].dline,logInst[startLog].res,logInst[startLog].eax,logInst[startLog].ebx,logInst[startLog].ecx,logInst[startLog].edx,logInst[startLog].esi,logInst[startLog].edi,logInst[startLog].ebp,logInst[startLog].esp,
		        logInst[startLog].s_ds,logInst[startLog].s_es,logInst[startLog].s_fs,logInst[startLog].s_gs,logInst[startLog].s_ss,
		        logInst[startLog].c,logInst[startLog].z,logInst[startLog].s,logInst[startLog].o,logInst[startLog].a,logInst[startLog].p,logInst[startLog].i);*/
		if (++startLog >= LOGCPUMAX) startLog = 0;
	} while (startLog != logCount);

	out.close();
	DEBUG_ShowMsg("DEBUG: Done.\n");
}

bool DEBUG_HeavyIsBreakpoint(void) {
#if defined(C_DOSBOX_AGENT)
	const bool agent_trace_was_active = agent_trace_active;
    if (agent_trace_active) {
		DEBUG_AgentCaptureTraceEvent();
		if (--agent_trace_remaining == 0) {
			agent_trace_active = false;
			DEBUG_EnableDebugger();
			return true;
		}
	}
#endif
#if C_DEBUG_LUA
 Debugger_ExecutionMaintain();
#endif
	if (cpuLog) {
		if (cpuLogCounter>0) {
			LogInstruction(SegValue(cs),reg_eip,cpuLogFile);
			cpuLogCounter--;
		}
		if (cpuLogCounter<=0) {
			cpuLogFile.flush();
			cpuLogFile.close();
			DEBUG_ShowMsg("DEBUG: cpu log LOGCPU.TXT created\n");
			cpuLog = false;
			DEBUG_EnableDebugger();
			return true;
		}
	}
	// LogInstruction
	if (logHeavy
#if defined (C_DOSBOX_AGENT)
        && !agent_trace_was_active
#endif
       ) DEBUG_HeavyLogInstruction();
	if (zeroProtect) {
		static Bitu zero_count = 0;
		uint32_t value = 0;
		if (!mem_readd_checked(SegPhys(cs)+reg_eip,&value)) {
			if (value == 0) zero_count++;
			else zero_count = 0;
		}
		if (GCC_UNLIKELY(zero_count == 10)) E_Exit("running zeroed code");
	}

	if (skipFirstInstruction) {
		skipFirstInstruction = false;
		return false;
	}
	if (!CBreakpoint::BPoints.empty() && CBreakpoint::CheckBreakpoint(SegValue(cs),reg_eip)) {
		return true;
	}
	return false;
}

/* this is for the BIOS, to stop the log upon BIOS POST. */
void DEBUG_StopLog(void) {
	if (cpuLog) {
        cpuLogCounter = 0;
        cpuLogFile.close();
        DEBUG_ShowMsg("DEBUG: cpu log LOGCPU.TXT stopped\n");
        cpuLog = false;
    }
}

#endif // HEAVY DEBUG


#endif // DEBUG
