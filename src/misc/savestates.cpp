#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <string>
#include <cstring>
#include <memory>
#include <fstream>
#include "SDL.h"
#include "menu.h"
#include "shell.h"
#include "cross.h"
#include "render.h"
#include "mapper.h"
#include "control.h"
#include "logging.h"
#include "mixer.h"
#include "build_timestamp.h"
#include "machine_service.h"
#include "state_machine_inputs.h"
#include "native_frontier.h"
#include "restored_session.h"
#include "dos_inc.h"
#if C_DEBUG && C_DEBUG_LUA
#include "../debug/debug_lua.h"
#endif
#include "debug.h"
#ifdef WIN32
#include "direct.h"
#endif
#if defined (__OpenBSD__) || defined (__APPLE__)
#else
#include <malloc.h>
#endif
#if defined(unix) || defined(__APPLE__)
# include <utime.h>
#endif

#define MAXU32 0xffffffff
#include "zip.h"
#include "unzip.h"
#include "ioapi.h"
#include "state_archive_output.h"
#include "state_checkpoint_stream.h"
#include <sstream>
#include <cstddef>
#include <limits>
#include "../hardware/capture_sha256.h"

// Representation/engine identity, not linked executable identity. Source and
// binary hashes belong to build receipts; debug symbols, Lua/Agent frontends
// and host SDL drivers do not change these common owner codecs. Increment the
// engine revision whenever their serialized semantics or reconstruction changes.
static std::string state_build_identity() {
    std::ostringstream identity;
    const uint32_t endian=0x01020304;
    identity << "original-owner-engine-8;endian=";
    const auto *bytes=reinterpret_cast<const unsigned char *>(&endian);
    for (size_t i=0;i<sizeof(endian);++i) identity << unsigned(bytes[i]) << ',';
    identity << ";char=" << sizeof(char) << ";short=" << sizeof(short)
             << ";int=" << sizeof(int) << ";long=" << sizeof(long)
             << ";pointer=" << sizeof(void*) << ";size=" << sizeof(size_t)
             << ";bool=" << sizeof(bool) << ";float=" << sizeof(float)
             << ";double=" << sizeof(double) << ";long-double=" << sizeof(long double)
             << ";max-align=" << alignof(std::max_align_t)
             << ";double-ieee=" << std::numeric_limits<double>::is_iec559
             << ";double-digits=" << std::numeric_limits<double>::digits;
#ifdef __GXX_ABI_VERSION
    identity << ";cxx-abi=" << __GXX_ABI_VERSION;
#endif
#ifdef _MSC_VER
    identity << ";msvc-abi=" << _MSC_VER;
#endif
#if C_FPU
    identity << ";fpu=1";
#else
    identity << ";fpu=0";
#endif
#if C_FPU_X86
    identity << ";fpu-x86=1";
#else
    identity << ";fpu-x86=0";
#endif
#if C_DYNAMIC_X86
    identity << ";dynamic-x86=1";
#else
    identity << ";dynamic-x86=0";
#endif
#if C_DYNREC
    identity << ";dynrec=1";
#else
    identity << ";dynrec=0";
#endif
    return identity.str();
}
static std::string state_config_identity() {
    if (STATE_MachineInputsIdentity().empty()) return {};
    CapturePNG::SHA256 hash;
    const auto field = [&](const std::string &s) {
        const std::string size = std::to_string(s.size()) + ":";
        hash.update(reinterpret_cast<const uint8_t *>(size.data()), size.size());
        hash.update(reinterpret_cast<const uint8_t *>(s.data()), s.size());
    };
    for (int i=0; auto *section=control->GetSection(i); ++i) {
        auto *properties=dynamic_cast<Section_prop *>(section);
        if (!properties) continue; // autoexec is launch policy, not machine ABI
        const std::string name=section->GetName();
        if (name=="sdl") continue;
        field(name);
        for (int j=0; auto *property=properties->Get_prop(j); ++j) {
            // Immutable ROM identity is contributed by the bytes installed
            // by the owning loader. A pathname is neither guest hardware nor
            // proof of those bytes; different paths may hold the same input.
            if ((name=="dosbox" && property->propname=="ibm rom basic") ||
                (name=="video" && property->propname=="vga bios rom image")) continue;
            // Destination presentation is rebuilt by SerializeRender. Keep
            // raster/font switches (char9/euro/doublescan/modeswitch) here
            // until their owning effective representation replaces this hash.
            // Scaler/aspect/frame skipping do not change guest hardware.
            if (name=="render" && (property->propname=="scaler" ||
                property->propname=="aspect" || property->propname=="aspect_ratio" ||
                property->propname=="frameskip")) continue;
            if (name=="dosbox" && (property->propname=="mcp_server" ||
                property->propname=="captures" || property->propname=="saves" ||
                property->propname=="compresssaveparts" ||
                property->propname=="saveslot" || property->propname=="savefile" ||
                property->propname=="saveremark" || property->propname=="autosave" ||
                property->propname=="forceloadstate")) continue;
            // RAM geometry is resolved (rounded/clamped) by Memory. Equivalent
            // requested sizes are not different hardware; its owning consumer
            // also checks allocated/handler/address/A20 geometry on install.
            if (name=="dosbox" && (property->propname=="memsize" ||
                                   property->propname=="memsizekb")) continue;
            field(property->propname);
            field(property->GetValue().ToString());
        }
    }
    field("resolved RAM pages");
    field(std::to_string(MEM_TotalPages()));
    field(std::to_string(MEM_TotalPagesAt4GB()));
    field(STATE_MachineInputsIdentity());
    char hex[65]; hash.finish(hex); return hex;
}
static const char state_archive_revision[] = "DOSBox-X original-state revision 8";

#include "vs/zlib/contrib/minizip/zip.c"
#include "vs/zlib/contrib/minizip/unzip.c"
#include "vs/zlib/contrib/minizip/ioapi.c"
#include "zipcppstdbuf.h"
#if !defined(HX_DOS)
#include "../libs/tinyfiledialogs/tinyfiledialogs.h"
#endif

#ifdef C_SDL2
extern SDL_AudioDeviceID SDL2_AudioDevice; /* valid IDs are 2 or higher, 1 for compat, 0 is never a valid ID */
#endif

extern unsigned int page;
extern int autosave_last[10], autosave_count;
extern std::string autosave_name[10], savefilename;
extern bool use_save_file, clearline, dos_kernel_disabled;
bool auto_save_state=false;
bool noremark_save_state = false;
bool force_load_state = false;
std::string saveloaderr="";
void refresh_slots(void);
void GFX_LosingFocus(void), GFX_ReleaseMouse(void), MAPPER_ReleaseAllKeys(void), resetFontSize(void);
bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);
extern std::string working_dir;

static bool save_state_frontier();

namespace
{
	std::string getTime(bool date=false)
	{
		const time_t current = time(NULL);
		tm* timeinfo;
		timeinfo = localtime(&current); //convert to local time
		char buffer[80];
		if (date)
			::strftime(buffer, 80, "%Y-%m-%d %H:%M", timeinfo);
		else
			::strftime(buffer, 50, "%H:%M:%S", timeinfo);
		return buffer;
	}

	std::string getType() {
		switch (machine) {
			case MCH_HERC:
				return "MCH_HERC";
			case MCH_CGA:
				return "MCH_CGA";
			case MCH_OLIVETTI:
				return "MCH_OLIVETTI";
			case MCH_3270PC:
				return "MCH_3270PC";
			case MCH_TANDY:
				return "MCH_TANDY";
			case MCH_PCJR:
				return "MCH_PCJR";
			case MCH_EGA:
				return "MCH_EGA";
			case MCH_VGA:
				return "MCH_VGA";
			case MCH_AMSTRAD:
				return "MCH_AMSTRAD";
			case MCH_PC98:
				return "MCH_PC98";
			case MCH_FM_TOWNS:
				return "MCH_FM_TOWNS";
			case MCH_MCGA:
				return "MCH_MCGA";
			case MCH_MDA:
				return "MCH_MDA";
			default:
				return "MCH_OTHER";
		}
	}

	size_t GetGameState();

	class SlotPos
	{
		public:
			SlotPos() : slot(0) {}

			void next()
			{
				++slot;
				slot %= SaveState::SLOT_COUNT*SaveState::MAX_PAGE;
				if (page!=GetGameState()/SaveState::SLOT_COUNT) {
					page=(unsigned int)GetGameState()/SaveState::SLOT_COUNT;
					refresh_slots();
				}
			}

			void previous()
			{
				slot += SaveState::SLOT_COUNT*SaveState::MAX_PAGE - 1;
				slot %= SaveState::SLOT_COUNT*SaveState::MAX_PAGE;
				if (page!=GetGameState()/SaveState::SLOT_COUNT) {
					page=(unsigned int)GetGameState()/SaveState::SLOT_COUNT;
					refresh_slots();
				}
			}

			void set(int value)
			{
				slot = value;
			}

			operator size_t() const
			{
				return slot;
			}
		private:
			size_t slot;
	};

	SlotPos currentSlot;

	void notifyError(const std::string& message, bool log=true)
	{
		if (log) LOG_MSG("%s",message.c_str());
		systemmessagebox("Error",message.c_str(),"ok","error", 1);
	}

	size_t GetGameState(void) {
		return currentSlot;
	}

	void SetGameState(int value) {
		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.set(value);
		if (page!=currentSlot/SaveState::SLOT_COUNT) {
			page=(unsigned int)(currentSlot/SaveState::SLOT_COUNT);
			refresh_slots();
		}
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1,  emptySlot ? "[Empty]" : "");
	}

	void SaveGameState(bool pressed) {
		if (!pressed) return;
        // Autosave can arrive inside VGA/PIC dispatch. Refuse before focus/key
        // release or UI work can pump events beneath that native callback.
        if (!save_state_frontier()) {
            LOG_MSG("Save refused: unsafe native continuation");
            return;
        }

		GFX_LosingFocus();

		try
		{
			//LOG_MSG("Saving state to slot: %d", (int)currentSlot + 1);
			SaveState::instance().save(currentSlot);
			if (page!=GetGameState()/SaveState::SLOT_COUNT)
				SetGameState((int)currentSlot);
			else
				refresh_slots();
		}
		catch (const SaveState::Error& err)
		{
			notifyError(err);
		}
	}


	void LoadGameState(bool pressed) {
		if (!pressed) return;

		//    if (SaveState::instance().isEmpty(currentSlot))
		//    {
		//        LOG_MSG("[%s]: State %d is empty!", getTime().c_str(), currentSlot + 1);
		//        return;
		//    }

		try
		{
			//LOG_MSG("Loading state from slot: %d", (int)currentSlot + 1);
			SaveState::instance().load(currentSlot);
		}
		catch (const SaveState::Error& err)
		{
			notifyError(err);
		}
	}

	void NextSaveSlot(bool pressed) {
		if (!pressed) return;

		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.next();
		if (currentSlot/SaveState::SLOT_COUNT==page) {
			name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
			mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		}

		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1, emptySlot ? "[Empty]" : "");
	}

	void PreviousSaveSlot(bool pressed) {
		if (!pressed) return;

		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.previous();
		if (currentSlot/SaveState::SLOT_COUNT==page) {
			name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
			mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		}

		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1, emptySlot ? "[Empty]" : "");
	}

	void LastAutoSaveSlot(bool pressed) {
		if (!pressed) return;
		int index=0;
		for (int i=1; i<10&&i<=autosave_count; i++) if (autosave_name[i].size()&&!strcasecmp(RunningProgram.c_str(), autosave_name[i].c_str())) index=i;
		if (autosave_last[index]<1) return;

		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.set(autosave_last[index]-1);
		if (page!=currentSlot/SaveState::SLOT_COUNT) {
			page=(unsigned int)(currentSlot/SaveState::SLOT_COUNT);
			refresh_slots();
		}
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(true).refresh_item(mainMenu);

		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1, emptySlot ? "[Empty]" : "");
	}
}

std::string GetPlatform(bool save) {
	char platform[30];
	strcpy(platform, 
#if defined(HX_DOS)
			"DOS "
#elif defined(__MINGW32__)
			"MinGW "
#elif defined(WIN32)
			"Windows "
#elif defined(LINUX)
			"Linux "
#elif unix
			"Unix "
#elif defined(MACOSX)
			"macOS "
#elif defined(OS2)
			"OS/2 "
#else
			save?"Other ":""
#endif
	      );
	if (!save) strcat(platform, (std::string(SDL_STRING)+", ").c_str());
#if defined(_M_X64) || defined (_M_AMD64) || defined (_M_ARM64) || defined (_M_IA64) || defined(__ia64__) || defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__powerpc64__)
	strcat(platform, "64");
#else
	strcat(platform, "32");
#endif
	strcat(platform, save?"-bit build":"-bit");
	return std::string(platform);
}

size_t GetGameState_Run(void) { return GetGameState(); }
void SetGameState_Run(int value) { SetGameState(value); }
void SaveGameState_Run(void) { SaveGameState(true); }
void LoadGameState_Run(void) { LoadGameState(true); }
void NextSaveSlot_Run(void) { NextSaveSlot(true); }
void PreviousSaveSlot_Run(void) { PreviousSaveSlot(true); }
void LastAutoSaveSlot_Run(void) { LastAutoSaveSlot(true); }

void ShowStateInfo(bool pressed) {
	if (!pressed) return;
	std::string message = "Save to: "+(use_save_file&&savefilename.size()?"File "+savefilename:"Slot "+std::to_string(GetGameState_Run()+1))+"\n"+SaveState::instance().getName(GetGameState_Run(), true);
	systemmessagebox("Saved state information", message.c_str(), "ok","info", 1);
}

void AddSaveStateMapper() {
	DOSBoxMenu::item *item;
	MAPPER_AddHandler(SaveGameState, MK_s, MMODHOST,"savestate","Save state", &item);
	item->set_text("Save state");
	MAPPER_AddHandler(LoadGameState, MK_l, MMODHOST,"loadstate","Load state", &item);
	item->set_text("Load state");
	MAPPER_AddHandler(ShowStateInfo, MK_nothing, 0,"showstate","Display state info", &item);
	item->set_text("Display state information");
	MAPPER_AddHandler(PreviousSaveSlot, MK_comma, MMODHOST,"prevslot","Previous save slot", &item);
	item->set_text("Select previous slot");
	MAPPER_AddHandler(NextSaveSlot, MK_period, MMODHOST,"nextslot","Next save slot", &item);
	item->set_text("Select next slot");
}

#ifndef WIN32
char* itoa(int value, char* str, int radix) {
	/**
	 * C++ version 0.4 char* style "itoa":
	 * Written by LukÃ¡s Chmela
	 * Released under GPLv3.
	 */
	// check that the radix if valid
	if (radix < 2 || radix > 36) { *str = '\0'; return str; }

	char* ptr = str, *ptr1 = str, tmp_char;
	int tmp_value;

	do {
		tmp_value = value;
		value /= radix;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * radix)];
	} while ( value );

	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while(ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return str;
}
#endif

SaveState& SaveState::instance() {
	static SaveState singleton;
	return singleton;
}

static bool save_state_frontier();

static std::map<std::string,std::string> state_identity() {
    return {{"Archive_Revision",state_archive_revision},
            {"Build_Identity",state_build_identity()},
            {"Config_Identity",state_config_identity()}};
}

// Both transports use this single original-engine dependency traversal. Each
// owner checks consumed state; failure is not a promise of destination rollback.
bool SaveState::capture(Output &output) const {
    // CONFIGSHELL may allocate callbacks and execute drivers before SHELL_Init.
    // That guest-executing route is not represented by the fresh recipe.
    const auto *devices=dynamic_cast<Section_line *>(control->GetSection("devices"));
    if (devices && devices->data.find_first_not_of(" \t")!=std::string::npos) {
        LOG_MSG("State capture unsupported: nonempty [devices] requires guest driver construction");
        return false;
    }
    for (const auto &item : state_identity()) {
        if (item.second.empty()) return false;
        auto *stream=output.open(item.first);
        if (!stream) return false;
        *stream << item.second;
        const bool complete=bool(*stream);
        if (!output.close() || !complete) return false;
    }
    for (const auto &entry : components) {
        auto *stream = output.open(entry.first);
        if (!stream) return false;
        entry.second.comp.getBytes(*stream);
        const bool complete = bool(*stream);
        if (!output.close() || !complete) return false;
    }
    return true;
}
bool SaveState::install(Input &input) const {
    for (const auto &item : state_identity()) {
        if (item.second.empty() || !input.verify(item.first)) {
            LOG_MSG("State compatibility metadata unavailable: %s",item.first.c_str());
            return false;
        }
        auto *stream=input.open(item.first);
        if (!stream) {
            LOG_MSG("State compatibility metadata cannot be opened: %s",item.first.c_str());
            return false;
        }
        std::string actual(item.second.size(),'\0');
        stream->read(&actual[0],actual.size());
        const bool match=bool(*stream) && actual==item.second &&
            stream->peek()==std::char_traits<char>::eof();
        const bool closed=input.close();
        if (!closed || !match) {
            LOG_MSG("State compatibility metadata %s: %s",item.first.c_str(),
                    !closed ? "settlement failed" : "incompatible or malformed");
            return false;
        }
    }
    // Owners consume progressively. Member EOF/CRC/close is settled below;
    // a late failure disposes the startup target, not a rollback promise.
#if C_DEBUG && C_DEBUG_LUA
    if (!Debugger_LuaPrepareRestore()) return false;
#endif
    bool started = false;
    try {
        for (const auto &entry : components) {
            auto *stream = input.open(entry.first);
            if (!stream) {
                if (started) E_Exit("State member unavailable after installation started");
                return false;
            }
            started = true;
            entry.second.comp.setBytes(*stream);
            if (!*stream || stream->peek()!=std::char_traits<char>::eof())
                E_Exit("State component %s failed during installation",entry.first.c_str());
            if (!input.close()) E_Exit("State member settlement failed after installation");
        }
    } catch (const std::exception &) {
        if (started) E_Exit("State installation failed after live state may have changed");
        return false;
    } catch (const SaveState::Error &) {
        if (started) E_Exit("State installation failed after live state may have changed");
        return false;
    }
#if C_DEBUG
    DEBUG_StateInstalled();
#endif
    return true;
}
struct SaveState::MemoryOutput final : Output {
    Checkpoint pending;
    size_t remaining;
    std::string name;
    std::unique_ptr<StateCheckpointStream> buffer;
    std::unique_ptr<std::ostream> stream;
    explicit MemoryOutput(size_t limit) : remaining(limit) {}
    std::ostream *open(const std::string &member) override {
        name=member;
        buffer.reset(new StateCheckpointStream(remaining));
        stream.reset(new std::ostream(buffer.get()));
        return stream.get();
    }
    bool close() override {
        if (buffer->failed()) return false;
        auto bytes=buffer->take();remaining-=bytes.size();
        pending.emplace(name,std::move(bytes));
        stream.reset();buffer.reset();return true;
    }
};
struct SaveState::MemoryInput final : Input {
    const Checkpoint &parts;
    std::unique_ptr<std::istringstream> stream;
    explicit MemoryInput(const Checkpoint &data) : parts(data) {}
    bool verify(const std::string &name) override {return parts.count(name);}
    std::istream *open(const std::string &name) override {
        stream.reset(new std::istringstream(parts.at(name),std::ios::in|std::ios::binary));
        return stream.get();
    }
    bool close() override {stream.reset();return true;}
};

bool SaveState::checkpointSave(Checkpoint &parts, size_t limit) {
    if (!save_state_frontier()) return false;
    try {
        MemoryOutput output(limit);
        if (!capture(output)) return false;
        parts.swap(output.pending);
        return true;
    } catch (const std::exception &) { return false; }
      catch (const SaveState::Error &) { return false; }
}

bool SaveState::checkpointLoad(const Checkpoint &parts) {
    (void)parts;
    LOG_MSG("State load refused: fresh launch required; used-target checkpoint restore is unsupported");
    return false;
}

bool SaveState::ComponentOrder::operator()(const std::string &left,
                                           const std::string &right) const {
    // Registration storage is also the common capture/install dependency order.
    // Memory owns mappings; DOS/XMS/EMS reconstruct their resources against it;
    // CPU selects decoders and resets caches only after those owners. Events
    // follow device state. Output owners are last. Unknown extensions remain
    // deterministic, but are not thereby qualified for fresh installation.
    static const char *const order[] = {
        "BIOS layout", "Memory", "Shell bindings", "Dos", "XMS", "EMS", "CPU", "FPU", "CMOS", "DMA",
        "IO handler", "Int10", "IntTimer10", "Keyboard", "Joystick",
        "Mouse", "Vga", "Mixer", "Midi", "Pic", "Dosbox-x", "Render"
    };
    auto rank = [&](const std::string &name) {
        size_t i = 0;
        for (; i < sizeof(order)/sizeof(order[0]); ++i)
            if (name == order[i]) break;
        return i;
    };
    const auto a = rank(left), b = rank(right);
    return a == b ? left < right : a < b;
}

void SaveState::registerComponent(const std::string& uniqueName, Component& comp) {
	components.insert(std::make_pair(uniqueName, CompData(comp)));
}

#define CASESENSITIVITY (0)
#define MAXFILENAME (256)

void zipSetCurrentTime(zip_fileinfo &zi) {
	zi.dosDate = 0;
	zi.internal_fa = 0;
	zi.external_fa = 0;
	zi.tmz_date.tm_sec = 0;
	zi.tmz_date.tm_min = 0;
	zi.tmz_date.tm_hour = 0;
	zi.tmz_date.tm_mday = 0;
	zi.tmz_date.tm_mon = 0;
	zi.tmz_date.tm_year = 0;

	time_t tm_t = time(NULL);
	struct tm* filedate = localtime(&tm_t);
	if (filedate != NULL) {
		zi.tmz_date.tm_sec  = filedate->tm_sec;
		zi.tmz_date.tm_min  = filedate->tm_min;
		zi.tmz_date.tm_hour = filedate->tm_hour;
		zi.tmz_date.tm_mday = filedate->tm_mday;
		zi.tmz_date.tm_mon  = filedate->tm_mon;
		zi.tmz_date.tm_year = filedate->tm_year;
	}
}

#ifdef __APPLE__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif


int zipOutOpenFile(zipFile zf,const char *zfname,zip_fileinfo &zi,const bool compress) {
	const int opt_compress_level = compress ? 9 : 0;

	return zipOpenNewFileInZip3_64(zf,zfname,&zi,
		NULL,0,NULL,0,NULL/* comment*/,
		(opt_compress_level != 0) ? Z_DEFLATED : 0,
		opt_compress_level,0,
		-MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
		NULL/*password*/,0/*crcFile*/,1/*zip64*/);
}

struct SaveState::ArchiveOutput final : Output {
    zipFile archive;
    bool compress;
    const StateArchive::Budget *budget;
    StateArchive::Effects *effects;
    std::unique_ptr<zip_ostreambuf> buffer;
    std::unique_ptr<std::ostream> stream;
    ArchiveOutput(zipFile file,bool compression,const StateArchive::Budget *clock,StateArchive::Effects *facts)
        : archive(file),compress(compression),budget(clock),effects(facts) {}
    std::ostream *open(const std::string &name) override {
        if (budget && budget->expired()) {
            if (effects) effects->timed_out=true;
            return nullptr;
        }
        zip_fileinfo zi;zipSetCurrentTime(zi);
        if (zipOutOpenFile(archive,name.c_str(),zi,compress)!=ZIP_OK) return nullptr;
        buffer.reset(new zip_ostreambuf(archive));
        stream.reset(new std::ostream(buffer.get()));
        return stream.get();
    }
    bool close() override {
        const bool ok=buffer->close()==ZIP_OK;
        stream.reset();buffer.reset();return ok;
    }
};
struct SaveState::ArchiveInput final : Input {
    unzFile archive;
    std::unique_ptr<zip_istreambuf> buffer;
    std::unique_ptr<std::istream> stream;
    explicit ArchiveInput(unzFile file) : archive(file) {}
    bool verify(const std::string &name) override {
        return zip_verify_member(archive,name.c_str())==UNZ_OK;
    }
    std::istream *open(const std::string &name) override {
        if (unzLocateFile(archive,name.c_str(),1)!=UNZ_OK ||
            unzOpenCurrentFile(archive)!=UNZ_OK) return nullptr;
        buffer.reset(new zip_istreambuf(archive));
        stream.reset(new std::istream(buffer.get()));
        return stream.get();
    }
    bool close() override {
        const bool ok=buffer->close()==UNZ_OK;
        stream.reset();buffer.reset();return ok;
    }
};

static bool save_state_frontier() {
    // No decoder, BIOS/DOS callback or idle-wrapper restoration suffix can
    // remain underneath installation. An audited owner grammar is also needed.
    const bool quiescent = NativeFrontier::frontiers().quiescent();
    const auto admission = DOSBOX_MachineContextAdmission();
    if (!quiescent || admission != MachineService::Denial::None) {
        const auto owner = DOSBOX_MachineServiceOwner();
        LOG_MSG("State frontier refused: quiescent=%u owner=%u depth=%u admission=%u",
                unsigned(quiescent), unsigned(owner.kind), owner.depth, unsigned(admission));
        return false;
    }
    return true;
}
void SaveState::save(size_t slot) {
    if (!save_state_frontier()) {
        LOG_MSG("Save refused: unsafe native continuation");
        return;
    }
    std::string remark = auto_save_state ? "Auto-save" : "";
#if !defined(HX_DOS)
    if (!auto_save_state && !noremark_save_state) {
        const bool fs=GFX_IsFullscreen();
        if (fs) GFX_SwitchFullScreen();
        MAPPER_ReleaseAllKeys(); GFX_LosingFocus(); GFX_ReleaseMouse();
        const char *text=tinyfd_inputBox("Save state", "Optional remark (30 characters maximum)", " ");
        if (text) remark=std::string(text).substr(0,30);
        MAPPER_ReleaseAllKeys(); GFX_LosingFocus();
        if (fs && !GFX_IsFullscreen()) GFX_SwitchFullScreen();
        if (!text) return;
    }
#endif
#if C_DEBUG
    if (!DEBUG_NativeStateSlot(slot,false,remark)) LOG_MSG("Slot save refused or failed");
#else
    if (!saveTo(slot,nullptr,nullptr,nullptr,remark)) LOG_MSG("Slot save refused or failed");
#endif
}
bool SaveState::saveFile(const std::string &path) { return saveTo(0, &path); }
bool SaveState::saveFile(const std::string &path, StateArchive::Effects &effects,
                         const StateArchive::Budget *budget) {
    effects = {};
    return saveTo(0, &path, &effects, budget);
}
bool SaveState::saveTo(size_t slot, const std::string *destination, StateArchive::Effects *effects,
                       const StateArchive::Budget *budget, const std::string &remark) {
    const auto expired = [&]() {
        if (!budget || !budget->expired()) return false;
        if (effects) effects->timed_out = true;
        return true;
    };
    if (expired()) return false;
    if (!save_state_frontier()) { LOG_MSG("Save refused: unsafe native continuation"); return false; }
	if (slot >= SLOT_COUNT*MAX_PAGE) return false;
	bool save_err=false;
	if((MEM_TotalPages()*4096/1024/1024)>1024) {
		LOG_MSG("Stopped. 1 GB is the maximum memory size for saving/loading states.");
		LOG_MSG("Unsupported memory size for saving states.");
		return false;
	}
	bool compresssaveparts = static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_bool("compresssaveparts");
	const char *save_remark = remark.c_str();
	int errclose;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(!Get_Custom_SaveDir(path)) {
        path = working_dir + CROSS_FILESPLIT + "save";
        Cross::CreateDir(path);
    }
    path += CROSS_FILESPLIT;

	std::string temp, save2;
	std::stringstream slotname;
	slotname << slot+1;
	temp=path;
	std::string save=destination ? *destination : (use_save_file&&savefilename.size()?savefilename:temp+slotname.str()+".sav");
    LOG_MSG("Saving state to slot: %d (%s)", (int)slot + 1, save.c_str());

    if (expired()) return false;
    StateArchiveOutput output(save, effects);
	zipFile zf = nullptr;
    StateArchiveZip archive(zf);
	{
		const char *global_comment = "DOSBox-X save state";
		zlib_filefunc64_def ffunc;
        if (output.prepare(ffunc))
		    zf = zipOpen2_64(save.c_str(),APPEND_STATUS_CREATE,&global_comment,&ffunc);
	}
	if (zf == NULL) { save_err = true; goto done; }


	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"DOSBox-X_Version",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream emulatorversion(&zos);

		emulatorversion << "DOSBox-X " << VERSION << " (" << SDL_STRING << ")" << std::endl << GetPlatform(true) << std::endl << UPDATED_STR;
		/* 2025/01/12: Backwards compat: The old code compressed data to zlib, even though the ZIP support code
		 *             already applies compression. This is to tell the old code that we did not compress the
		 *             data (the ZIP support code did though). */
		emulatorversion << std::endl << "No compression";

		if (!emulatorversion) save_err = true;
		if ((errclose=zos.close()) != ZIP_OK) save_err = true;
		if (save_err) goto done;
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Program_Name",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream programname(&zos);

		programname << RunningProgram;

		if (!programname) save_err = true;
		if ((errclose=zos.close()) != ZIP_OK) save_err = true;
		if (save_err) goto done;
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Memory_Size",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream memorysize(&zos);

		memorysize << std::to_string( MEM_TotalPages());

		if (!memorysize) save_err = true;
		if ((errclose=zos.close()) != ZIP_OK) save_err = true;
		if (save_err) goto done;
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Machine_Type",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream machinetype(&zos);

		machinetype << getType();

		if (!machinetype) save_err = true;
		if ((errclose=zos.close()) != ZIP_OK) save_err = true;
		if (save_err) goto done;
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Time_Stamp",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream timestamp(&zos);

		timestamp << getTime(true);

		if (!timestamp) save_err = true;
		if ((errclose=zos.close()) != ZIP_OK) save_err = true;
		if (save_err) goto done;
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Save_Remark",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream saveremark(&zos);

		saveremark << std::string(save_remark);

		if (!saveremark) save_err = true;
		if ((errclose=zos.close()) != ZIP_OK) save_err = true;
		if (save_err) goto done;
	}
	{
        ArchiveOutput members(zf,compresssaveparts,budget,effects);
        if (!capture(members)) save_err=true;
    }

done:
	if (zf != NULL) {
		errclose = archive.close();
		if (errclose != ZIP_OK) save_err = true;
	}

    if (expired()) save_err = true;
    // Machine archives preserve DOS bookkeeping, not external file contents.
    // Flag registration does not enroll host files in content rollback.
    if (expired()) save_err = true;
    if (!save_err && !output.publish()) save_err = true;

	if (save_err) {
        LOG_MSG("%s", MSG_Get("SAVE_FAILED"));
    }
	else
		LOG_MSG("[%s]: Saved. (Slot %d)", getTime().c_str(), (int)slot+1);
    return !save_err;
}

void savestatecorrupt(const char* part) {
	LOG_MSG("Save state corrupted! Program in inconsistent state! - %s", part);
	systemmessagebox("Error", MSG_Get("SAVE_CORRUPTED"),"ok","error", 1);
}

bool confres=false;
std::string loadstate_detail_saved;
std::string loadstate_detail_current;
bool loadstateconfirm(int ind) {
	if (ind<0||ind>4) return false;
	confres=true;
	GUI_Shortcut(23+ind);
	bool ret=confres;
	confres=false;
	return ret;
}

void SaveState::load(size_t slot) const {
    (void)loadFrom(slot,nullptr);
}
bool SaveState::loadFile(const std::string &path) const { return loadFrom(0, &path); }

void SaveState::restoreFreshStartup(const std::string &path) const {
    static bool attempted=false;
    if (attempted || path.empty() || !NativeFrontier::frontiers().quiescent() ||
        DOSBOX_MachineServiceOwner().token)
        E_Exit("Fresh restore requires unused startup outside guest frames");
    attempted=true;
    extern void VM_ConstructFreshNative();
    VM_ConstructFreshNative();
    unzFile archive=unzOpen64(path.c_str());
    if (!archive) E_Exit("Fresh restore failed: cannot open source archive");
    // Settle the exact archive once, including exceptions after partial effects.
    // The outer startup owner must dispose; no boot fallback is permitted.
    bool installed=false;
    try {
        ArchiveInput input(archive);
        installed=install(input);
    } catch (...) {
        // Preserve the original failure, including a second settlement failure.
        if (unzClose(archive)!=UNZ_OK)
            LOG_MSG("Fresh restore archive close also failed; target must be disposed");
        throw;
    }
    const bool closed=unzClose(archive)==UNZ_OK;
    if (!installed || !closed)
        E_Exit("Fresh restore failed; disposable target cannot continue");
    const uint16_t root=dos.psp();
    if (!RestoredSession::validRoot(root,false) || RestoredSession::selected())
        E_Exit("Fresh restore failed: invalid or already selected native root");
}
bool SaveState::loadFrom(size_t slot, const std::string *source) const { //throw (Error)
    LOG_MSG("State load refused: fresh launch required (%s, slot %u)",
            source ? source->c_str() : "selected state archive", unsigned(slot + 1));
    return false;
}

bool SaveState::isEmpty(size_t slot) const {
	if (slot >= SLOT_COUNT*MAX_PAGE) return true;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	return check_slot.fail();
}

void SaveState::removeState(size_t slot) const {
	if (slot >= SLOT_COUNT*MAX_PAGE) return;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	if(check_slot.fail()) {
		LOG_MSG("No saved slot - %d (%s)",(int)slot+1,save.c_str());
		notifyError("The selected save slot is an empty slot.", false);
		return;
	}
	if (loadstateconfirm(4)) {
		check_slot.close();
		remove(save.c_str());
		check_slot.open(save.c_str(), std::ifstream::in);
		if (!check_slot.fail()) notifyError("Failed to remove the state in the save slot.");
		if (page!=GetGameState()/SaveState::SLOT_COUNT)
			SetGameState((int)slot);
		else
			refresh_slots();
	}
}

std::string SaveState::getName(size_t slot, bool nl) const {
	if (slot >= SLOT_COUNT*MAX_PAGE) return "["+std::string(MSG_Get("EMPTY_SLOT"))+"]";
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=nl&&use_save_file&&savefilename.size()?savefilename:temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	if (check_slot.fail()) return nl?"(Empty state)":"["+std::string(MSG_Get("EMPTY_SLOT"))+"]";
	check_slot.close();

	unzFile zf;
	{
		zlib_filefunc64_def ffunc;
#ifdef USEWIN32IOAPI
		fill_win32_filefunc64A(&ffunc);
#else
		fill_fopen64_filefunc(&ffunc);
#endif
		zf = unzOpen2_64(save.c_str(),&ffunc);
	}
	if (zf == NULL) return "(Error slot)";

	int err;
	std::string ret;
	char buffer1[4096];
	unz_file_info64 file_info;

	if (	(err=unzLocateFile(zf,"Program_Name",1/*case sensitive*/)) == UNZ_OK &&
		(err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) == UNZ_OK) {
		if ((err=unzOpenCurrentFile(zf)) == UNZ_OK) {
			zip_istreambuf zis(zf);
			size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer1,sizeof(buffer1)-1); buffer1[length] = 0;
			zis.close();
			ret += nl?"Program: "+(!strlen(buffer1)?"-":std::string(buffer1))+"\n":"[Program: "+std::string(buffer1)+"]";
		}
	}

	if (	(err=unzLocateFile(zf,"Time_Stamp",1/*case sensitive*/)) == UNZ_OK &&
		(err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) == UNZ_OK) {
		if ((err=unzOpenCurrentFile(zf)) == UNZ_OK) {
			zip_istreambuf zis(zf);
			size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer1,sizeof(buffer1)-1); buffer1[length] = 0;
			zis.close();
			ret += nl?"Timestamp: "+(!strlen(buffer1)?"-":std::string(buffer1))+"\n":" ("+std::string(buffer1)+")";
		}
	}

	if (	(err=unzLocateFile(zf,"Save_Remark",1/*case sensitive*/)) == UNZ_OK &&
		(err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) == UNZ_OK) {
		if ((err=unzOpenCurrentFile(zf)) == UNZ_OK) {
			zip_istreambuf zis(zf);
			size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer1,sizeof(buffer1)-1); buffer1[length] = 0;
			zis.close();
			if (length != 0) ret += nl?"Remark: "+(!strlen(buffer1)?"-":std::string(buffer1))+"\n":" - "+std::string(buffer1);
		}
	}
    unzClose(zf);
	return ret;
}
