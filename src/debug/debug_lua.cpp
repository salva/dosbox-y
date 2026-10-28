/*
 *  Copyright (C) 2002-2026  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <stdint.h>
#include <signal.h>
#include <sys/stat.h>

#include "dosbox.h"
#include "midi.h"
#if C_DEBUG_LUA

#include "lua_driver_handoff.h"
#include "debug_lua.h"

#include "debug_common.h"
#include "debug.h"
#include "video.h"
#include "logging.h"
#include "machine_service.h"
#include "native_transport.h"
#include "native_startup.h"
#include "lua_driver_options.h"
#include "native_state_root.h"
#include "native_control_owner.h"
#include "native_frontier.h"
#include "state_archive_effects.h"
#include "../hardware/capture_sha256.h"
#include "live_control.h"
#include "dos_inc.h"
#include "dos_system.h"
#include "../dos/drives.h"
#include "cpu.h"
#include "../cpu/lazyflags.h"
#include "mem.h"
#include <typeinfo>
// Standalone diagnostics belong to the selected interactive session.
static NativeDebug::Attachment local_attachment;
static NativeDebug::Attachment interactive_attachment;
static bool interactive_active=false;
// Live carrier owns a distinct exclusive session through evaluator close.
static NativeDebug::Attachment live_attachment;
extern bool KEYBOARD_CheckpointNeutral();
extern bool KEYBOARD_FileCheckpointReady();
extern void DOS_CheckpointSearchInventory(bool&,bool&,bool&,bool&);
extern const char *DOS_CheckpointConsoleRejection();
extern bool Mouse_CheckpointNeutral();
#include "native_ownership.h"
#include "pic.h"
#include <chrono>
#include <assert.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <memory>
#include <exception>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#endif

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

static lua_State *interactive_lua=nullptr;
static int LocalEvaluateFile(lua_State *L);

static void Debugger_LuaPushVersion(lua_State *L)
{
	const DebuggerVersion version = Debugger_GetVersion();

	lua_newtable(L);
	lua_pushliteral(L, "0.1.0");
	lua_setfield(L, -2, "api");
	lua_pushliteral(L, "DOSBox-X");
	lua_setfield(L, -2, "emulator");
	lua_pushstring(L, version.backend);
	lua_setfield(L, -2, "emulator_version");
	lua_pushliteral(L, "5.4");
	lua_setfield(L, -2, "lua");
	lua_pushliteral(L, "debug-lua");
	lua_setfield(L, -2, "build");
}

static void Debugger_LuaSetIntegerField(lua_State *L, const char *name, const lua_Integer value)
{
	lua_pushinteger(L, value);
	lua_setfield(L, -2, name);
}

static void Debugger_LuaSetBooleanField(lua_State *L, const char *name, const bool value)
{
	lua_pushboolean(L, value);
	lua_setfield(L, -2, name);
}

static uint8_t Debugger_LuaLowByte(const uint32_t value)
{
	return static_cast<uint8_t>(value & 0xff);
}

static uint8_t Debugger_LuaHighByte(const uint32_t value)
{
	return static_cast<uint8_t>((value >> 8) & 0xff);
}

static uint16_t Debugger_LuaLowWord(const uint32_t value)
{
	return static_cast<uint16_t>(value & 0xffff);
}


static bool Debugger_LuaNameEquals(const char *lhs, const char *rhs);

static std::string debugger_lua_capture_dir;

#if (defined(__unix__) || defined(__APPLE__)) && defined(O_NOFOLLOW) && defined(O_DIRECTORY) && defined(AT_SYMLINK_NOFOLLOW)
#define DEBUGGER_LUA_POSIX_ARTIFACT_WRITE 1
#endif

#if DEBUGGER_LUA_POSIX_ARTIFACT_WRITE
static int debugger_lua_capture_dir_fd = -1;

class DebuggerLuaUniqueFd {
public:
	DebuggerLuaUniqueFd() = default;
	explicit DebuggerLuaUniqueFd(const int new_fd) : fd(new_fd) {}
	~DebuggerLuaUniqueFd()
	{
		reset();
	}

	DebuggerLuaUniqueFd(const DebuggerLuaUniqueFd &) = delete;
	DebuggerLuaUniqueFd &operator=(const DebuggerLuaUniqueFd &) = delete;

	DebuggerLuaUniqueFd(DebuggerLuaUniqueFd &&other) noexcept : fd(other.fd)
	{
		other.fd = -1;
	}

	DebuggerLuaUniqueFd &operator=(DebuggerLuaUniqueFd &&other) noexcept
	{
		if (this != &other) {
			reset();
			fd = other.fd;
			other.fd = -1;
		}
		return *this;
	}

	int get() const
	{
		return fd;
	}

	bool valid() const
	{
		return fd >= 0;
	}

	int release()
	{
		const int released_fd = fd;
		fd = -1;
		return released_fd;
	}

	void reset(const int new_fd = -1)
	{
		if (fd >= 0)
			close(fd);
		fd = new_fd;
	}

private:
	int fd = -1;
};

static void Debugger_LuaSetCaptureDirFd(const int new_fd)
{
	if (debugger_lua_capture_dir_fd >= 0)
		close(debugger_lua_capture_dir_fd);
	debugger_lua_capture_dir_fd = new_fd;
}
#endif

static bool Debugger_LuaStringHasNul(const char *text, const size_t length)
{
	return text == NULL || memchr(text, '\0', length) != NULL;
}

static bool Debugger_LuaIsSeparator(const char ch)
{
	return ch == '/' || ch == '\\';
}

static std::string Debugger_LuaHostError(void)
{
	return strerror(errno);
}

static bool Debugger_LuaIsAbsoluteArtifactPath(const char *path, const size_t length)
{
	if (length == 0)
		return false;
	if (Debugger_LuaIsSeparator(path[0]))
		return true;
	if (length >= 2 && isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':')
		return true;
	return false;
}

#if !DEBUGGER_LUA_POSIX_ARTIFACT_WRITE
static bool Debugger_LuaPathIsContained(const std::string &root, const std::string &path)
{
	if (path == root)
		return true;
	if (path.size() <= root.size() || path.compare(0, root.size(), root) != 0)
		return false;
	return root.empty() || root[root.size() - 1] == '/' || path[root.size()] == '/';
}
#endif

static std::string Debugger_LuaJoinPath(const std::string &base, const std::string &component)
{
	if (base.empty() || base[base.size() - 1] == '/')
		return base + component;
	return base + "/" + component;
}

static int Debugger_LuaCheckStoppedWithMessage(lua_State *L, const char *message)
{
	const DebuggerStatus status = Debugger_GetStatus();
	if (!status.stopped)
		return luaL_error(L, "%s", message);
	return 0;
}

static bool Debugger_LuaEnsureDirectory(lua_State *L, const std::string &path, const char *error_prefix)
{
	if (path.empty())
		return false;

	std::string current;
	size_t pos = 0;
	if (path[0] == '/') {
		current = "/";
		pos = 1;
	}

	while (pos < path.size()) {
		const size_t slash = path.find('/', pos);
		const std::string component = path.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);
		if (!component.empty()) {
			current = Debugger_LuaJoinPath(current, component);

			struct stat st = {};
			if (lstat(current.c_str(), &st) == 0) {
				if (S_ISLNK(st.st_mode)) {
					luaL_error(L, "%s: symlink path is not allowed", error_prefix);
					return false;
				}
				if (!S_ISDIR(st.st_mode)) {
					luaL_error(L, "%s: path is not a directory", error_prefix);
					return false;
				}
			} else if (errno == ENOENT) {
				if (mkdir(current.c_str(), 0700) != 0 && errno != EEXIST) {
					const std::string error = Debugger_LuaHostError();
					luaL_error(L, "%s: failed to create directory: %s", error_prefix, error.c_str());
					return false;
				}
			} else {
				const std::string error = Debugger_LuaHostError();
				luaL_error(L, "%s: failed to create directory: %s", error_prefix, error.c_str());
				return false;
			}
		}

		if (slash == std::string::npos)
			break;
		pos = slash + 1;
	}

	return true;
}

static std::string Debugger_LuaCanonicalPath(lua_State *L, const std::string &path, const char *error_prefix)
{
	char resolved[PATH_MAX] = {};
	if (realpath(path.c_str(), resolved) == NULL) {
		const std::string error = Debugger_LuaHostError();
		luaL_error(L, "%s: %s", error_prefix, error.c_str());
		return std::string();
	}
	return resolved;
}

static std::vector<std::string> Debugger_LuaCheckArtifactComponents(lua_State *L, const int index)
{
	if (lua_type(L, index) != LUA_TSTRING)
		luaL_error(L, "invalid artifact path: path must be a string");

	size_t length = 0;
	const char *path = lua_tolstring(L, index, &length);
	if (length == 0)
		luaL_error(L, "invalid artifact path: path is empty");
	if (Debugger_LuaStringHasNul(path, length))
		luaL_error(L, "invalid artifact path: path contains NUL byte");
	if (Debugger_LuaIsAbsoluteArtifactPath(path, length))
		luaL_error(L, "invalid artifact path: absolute paths are not allowed");
	if (Debugger_LuaIsSeparator(path[length - 1]))
		luaL_error(L, "invalid artifact path: directory path is not allowed");

	std::vector<std::string> components;
	std::string component;
	for (size_t i = 0; i < length; ++i) {
		const char ch = path[i];
		if (Debugger_LuaIsSeparator(ch)) {
			if (component.empty())
				luaL_error(L, "invalid artifact path: traversal is not allowed");
			if (component == ".")
				luaL_error(L, "invalid artifact path: current-directory components are not allowed");
			if (component == "..")
				luaL_error(L, "invalid artifact path: traversal is not allowed");
			components.push_back(component);
			component.clear();
		} else {
			component += ch;
		}
	}
	if (component.empty())
		luaL_error(L, "invalid artifact path: directory path is not allowed");
	if (component == ".")
		luaL_error(L, "invalid artifact path: current-directory components are not allowed");
	if (component == "..")
		luaL_error(L, "invalid artifact path: traversal is not allowed");
	components.push_back(component);
	return components;
}

#if DEBUGGER_LUA_POSIX_ARTIFACT_WRITE
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

struct DebuggerLuaArtifactTarget {
	DebuggerLuaUniqueFd parent_fd;
	std::string leaf;
};

static DebuggerLuaArtifactTarget Debugger_LuaOpenArtifactParent(lua_State *L, const int index)
{
	if (debugger_lua_capture_dir.empty() || debugger_lua_capture_dir_fd < 0)
		luaL_error(L, "capture directory is not configured");

	const std::vector<std::string> components = Debugger_LuaCheckArtifactComponents(L, index);
	DebuggerLuaUniqueFd current_fd(dup(debugger_lua_capture_dir_fd));
	if (!current_fd.valid()) {
		const std::string error = Debugger_LuaHostError();
		luaL_error(L, "artifact write failed: %s", error.c_str());
	}

	for (size_t i = 0; i + 1 < components.size(); ++i) {
		const char *component = components[i].c_str();
		int next_fd = openat(current_fd.get(), component, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		if (next_fd < 0 && errno == ENOENT) {
			if (mkdirat(current_fd.get(), component, 0700) != 0 && errno != EEXIST) {
				const std::string error = Debugger_LuaHostError();
				luaL_error(L, "artifact write failed: %s", error.c_str());
			}
			next_fd = openat(current_fd.get(), component, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		}
		if (next_fd < 0) {
			struct stat failed_st = {};
			if (fstatat(current_fd.get(), component, &failed_st, AT_SYMLINK_NOFOLLOW) == 0) {
				if (S_ISLNK(failed_st.st_mode))
					luaL_error(L, "invalid artifact path: symlink path is not allowed");
				if (!S_ISDIR(failed_st.st_mode))
					luaL_error(L, "invalid artifact path: path is not a directory");
			}
			if (errno == ELOOP)
				luaL_error(L, "invalid artifact path: symlink path is not allowed");
			if (errno == ENOTDIR)
				luaL_error(L, "invalid artifact path: path is not a directory");
			const std::string error = Debugger_LuaHostError();
			luaL_error(L, "artifact write failed: %s", error.c_str());
		}

		DebuggerLuaUniqueFd verified_fd(next_fd);
		struct stat st = {};
		if (fstat(verified_fd.get(), &st) != 0) {
			const std::string error = Debugger_LuaHostError();
			luaL_error(L, "artifact write failed: %s", error.c_str());
		}
		if (!S_ISDIR(st.st_mode))
			luaL_error(L, "invalid artifact path: path is not a directory");
		current_fd = std::move(verified_fd);
	}

	DebuggerLuaArtifactTarget target;
	target.parent_fd = std::move(current_fd);
	target.leaf = components.back();
	return target;
}

static void Debugger_LuaCheckPosixArtifactFinal(lua_State *L, const int parent_fd, const char *leaf)
{
	struct stat st = {};
	if (fstatat(parent_fd, leaf, &st, AT_SYMLINK_NOFOLLOW) == 0) {
		if (S_ISLNK(st.st_mode))
			luaL_error(L, "invalid artifact path: symlink path is not allowed");
		if (S_ISDIR(st.st_mode))
			luaL_error(L, "invalid artifact path: directory path is not allowed");
		if (!S_ISREG(st.st_mode))
			luaL_error(L, "invalid artifact path: target is not a regular file");
		return;
	}
	if (errno != ENOENT) {
		const std::string error = Debugger_LuaHostError();
		luaL_error(L, "artifact write failed: %s", error.c_str());
	}
}

static bool Debugger_LuaWriteAllFd(const int fd, const char *data, const size_t length)
{
	size_t written = 0;
	while (written < length) {
		const ssize_t result = write(fd, data + written, length - written);
		if (result < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		if (result == 0) {
			errno = EIO;
			return false;
		}
		written += static_cast<size_t>(result);
	}
	return true;
}

static int Debugger_LuaWriteArtifactDataPosix(lua_State *L, const int path_index, const char *data, const size_t length)
{
	Debugger_LuaCheckStoppedWithMessage(L, "artifact write requires debugger-safe stopped state");
	DebuggerLuaArtifactTarget target = Debugger_LuaOpenArtifactParent(L, path_index);
	Debugger_LuaCheckPosixArtifactFinal(L, target.parent_fd.get(), target.leaf.c_str());

	char temp_name[64] = {};
	DebuggerLuaUniqueFd temp_fd;
	for (unsigned int attempt = 0; attempt < 100; ++attempt) {
		snprintf(temp_name, sizeof(temp_name), ".lua-debugger-tmp-%ld-%u", static_cast<long>(getpid()), attempt);
		temp_fd.reset(openat(target.parent_fd.get(), temp_name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
		if (temp_fd.valid())
			break;
		if (errno != EEXIST) {
			const std::string error = Debugger_LuaHostError();
			return luaL_error(L, "artifact write failed: %s", error.c_str());
		}
	}
	if (!temp_fd.valid())
		return luaL_error(L, "artifact write failed: could not create temporary file");

	if (!Debugger_LuaWriteAllFd(temp_fd.get(), data, length)) {
		const std::string error = Debugger_LuaHostError();
		unlinkat(target.parent_fd.get(), temp_name, 0);
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}
	if (close(temp_fd.release()) != 0) {
		const std::string error = Debugger_LuaHostError();
		unlinkat(target.parent_fd.get(), temp_name, 0);
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}

	struct stat final_st = {};
	if (fstatat(target.parent_fd.get(), target.leaf.c_str(), &final_st, AT_SYMLINK_NOFOLLOW) == 0) {
		if (S_ISLNK(final_st.st_mode)) {
			unlinkat(target.parent_fd.get(), temp_name, 0);
			return luaL_error(L, "invalid artifact path: symlink path is not allowed");
		}
		if (S_ISDIR(final_st.st_mode)) {
			unlinkat(target.parent_fd.get(), temp_name, 0);
			return luaL_error(L, "invalid artifact path: directory path is not allowed");
		}
		if (!S_ISREG(final_st.st_mode)) {
			unlinkat(target.parent_fd.get(), temp_name, 0);
			return luaL_error(L, "invalid artifact path: target is not a regular file");
		}
	} else if (errno != ENOENT) {
		const std::string error = Debugger_LuaHostError();
		unlinkat(target.parent_fd.get(), temp_name, 0);
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}
	if (renameat(target.parent_fd.get(), temp_name, target.parent_fd.get(), target.leaf.c_str()) != 0) {
		const std::string error = Debugger_LuaHostError();
		unlinkat(target.parent_fd.get(), temp_name, 0);
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}

	lua_pushboolean(L, true);
	return 1;
}
#else
static std::string Debugger_LuaPrepareArtifactPath(lua_State *L, const int index)
{
	if (debugger_lua_capture_dir.empty())
		luaL_error(L, "capture directory is not configured");

	const std::vector<std::string> components = Debugger_LuaCheckArtifactComponents(L, index);

	std::string parent = debugger_lua_capture_dir;
	for (size_t i = 0; i + 1 < components.size(); ++i) {
		parent = Debugger_LuaJoinPath(parent, components[i]);
		struct stat st = {};
		if (lstat(parent.c_str(), &st) == 0) {
			if (S_ISLNK(st.st_mode))
				luaL_error(L, "invalid artifact path: symlink path is not allowed");
			if (!S_ISDIR(st.st_mode))
				luaL_error(L, "invalid artifact path: path is not a directory");
		} else if (errno == ENOENT) {
			if (mkdir(parent.c_str(), 0700) != 0 && errno != EEXIST) {
				const std::string error = Debugger_LuaHostError();
				luaL_error(L, "artifact write failed: %s", error.c_str());
			}
		} else {
			const std::string error = Debugger_LuaHostError();
			luaL_error(L, "artifact write failed: %s", error.c_str());
		}
	}

	const std::string canonical_parent = Debugger_LuaCanonicalPath(L, parent, "artifact write failed");
	if (!Debugger_LuaPathIsContained(debugger_lua_capture_dir, canonical_parent))
		luaL_error(L, "invalid artifact path: path escapes capture directory");

	const std::string final_path = Debugger_LuaJoinPath(canonical_parent, components.back());
	struct stat st = {};
	if (lstat(final_path.c_str(), &st) == 0) {
		if (S_ISLNK(st.st_mode))
			luaL_error(L, "invalid artifact path: symlink path is not allowed");
		if (S_ISDIR(st.st_mode))
			luaL_error(L, "invalid artifact path: directory path is not allowed");
		if (!S_ISREG(st.st_mode))
			luaL_error(L, "invalid artifact path: target is not a regular file");
	} else if (errno != ENOENT) {
		const std::string error = Debugger_LuaHostError();
		luaL_error(L, "artifact write failed: %s", error.c_str());
	}

	if (!Debugger_LuaPathIsContained(debugger_lua_capture_dir, final_path))
		luaL_error(L, "invalid artifact path: path escapes capture directory");
	return final_path;
}

static int Debugger_LuaWriteArtifactDataFallback(lua_State *L, const int path_index, const char *data, const size_t length)
{
	Debugger_LuaCheckStoppedWithMessage(L, "artifact write requires debugger-safe stopped state");
	const std::string final_path = Debugger_LuaPrepareArtifactPath(L, path_index);
	const std::string parent = final_path.substr(0, final_path.find_last_of('/'));
	const std::string temp_template = Debugger_LuaJoinPath(parent, ".lua-debugger-tmp-XXXXXX");

	std::vector<char> temp_path(temp_template.begin(), temp_template.end());
	temp_path.push_back('\0');
	const int fd = mkstemp(temp_path.data());
	if (fd < 0) {
		const std::string error = Debugger_LuaHostError();
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}

	FILE *file = fdopen(fd, "wb");
	if (file == NULL) {
		const std::string error = Debugger_LuaHostError();
		remove(temp_path.data());
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}
	if (length > 0 && fwrite(data, 1, length, file) != length) {
		const std::string error = ferror(file) ? Debugger_LuaHostError() : std::string("short write");
		fclose(file);
		remove(temp_path.data());
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}
	if (fflush(file) != 0 || fclose(file) != 0) {
		const std::string error = Debugger_LuaHostError();
		remove(temp_path.data());
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}
	if (rename(temp_path.data(), final_path.c_str()) != 0) {
		const std::string error = Debugger_LuaHostError();
		remove(temp_path.data());
		return luaL_error(L, "artifact write failed: %s", error.c_str());
	}

	lua_pushboolean(L, true);
	return 1;
}
#endif

static int Debugger_LuaWriteArtifactData(lua_State *L, const int path_index, const char *data, const size_t length)
{
#if DEBUGGER_LUA_POSIX_ARTIFACT_WRITE
	return Debugger_LuaWriteArtifactDataPosix(L, path_index, data, length);
#else
	return Debugger_LuaWriteArtifactDataFallback(L, path_index, data, length);
#endif
}

static bool Debugger_LuaIntegerValue(lua_State *L, const int index, uint32_t &value)
{
	if (lua_type(L, index) != LUA_TNUMBER)
		return false;

	int is_number = 0;
	const lua_Integer integer = lua_tointegerx(L, index, &is_number);
	if (!is_number || integer < 0 || static_cast<uint64_t>(integer) > 0xfffffffful)
		return false;

	value = static_cast<uint32_t>(integer);
	return true;
}

static int Debugger_LuaCheckAddressPart(lua_State *L, const int index, const char *name, const uint32_t max)
{
	int is_number = 0;
	const lua_Integer integer = lua_tointegerx(L, index, &is_number);
	if (!is_number)
		return luaL_error(L, "invalid address: %s must be numeric", name);
	if (integer < 0 || static_cast<uint64_t>(integer) > max)
		return luaL_error(L, "invalid address: %s out of range", name);

	return static_cast<int>(integer);
}

static bool Debugger_LuaParseHexPart(const char *text, uint32_t &value)
{
	if (text == NULL || text[0] == '\0')
		return false;

	char *end = NULL;
	const unsigned long parsed = strtoul(text, &end, 16);
	if (end == text || *end != '\0')
		return false;

	value = static_cast<uint32_t>(parsed);
	return parsed <= 0xfffffffful;
}

static bool Debugger_LuaIsAlphaString(const char *text)
{
	if (text == NULL || text[0] == '\0')
		return false;

	for (const char *p = text; *p != '\0'; ++p) {
		if (!isalpha(static_cast<unsigned char>(*p)))
			return false;
	}
	return true;
}

static bool Debugger_LuaRegisterPair(const char *text, const DebuggerRegisters &regs, uint16_t &seg, uint16_t &off)
{
	struct RegisterPair {
		const char *name;
		uint16_t seg;
		uint16_t off;
	};

	const RegisterPair pairs[] = {
	        {"CS:IP", regs.cs, regs.ip},
	        {"DS:DX", regs.ds, Debugger_LuaLowWord(regs.edx)},
	        {"ES:DI", regs.es, Debugger_LuaLowWord(regs.edi)},
	        {"SS:SP", regs.ss, Debugger_LuaLowWord(regs.esp)},
	        {"DS:SI", regs.ds, Debugger_LuaLowWord(regs.esi)},
	        {"ES:BX", regs.es, Debugger_LuaLowWord(regs.ebx)},
	        {"CS:SI", regs.cs, Debugger_LuaLowWord(regs.esi)},
	        {"DS:BX", regs.ds, Debugger_LuaLowWord(regs.ebx)},
	};

	for (const auto &pair : pairs) {
		if (Debugger_LuaNameEquals(text, pair.name)) {
			seg = pair.seg;
			off = pair.off;
			return true;
		}
	}

	return false;
}

static int Debugger_LuaResolveAddress(lua_State *L, const uint16_t seg, const uint16_t off)
{
	const DebuggerAddress address = Debugger_ResolveAddress(seg, off);
	if (!address.valid)
		return luaL_error(L, "invalid address: protected-mode selector/offset is not addressable");

	char text[16] = {};
	if (!Debugger_FormatAddress(text, sizeof(text), address.seg, address.off))
		return luaL_error(L, "invalid address: failed to format address");

	lua_newtable(L);
	Debugger_LuaSetIntegerField(L, "seg", address.seg);
	Debugger_LuaSetIntegerField(L, "off", address.off);
	Debugger_LuaSetIntegerField(L, "resolved", static_cast<lua_Integer>(address.resolved));
	lua_pushstring(L, address.resolved_kind == DebuggerAddressKind::Linear ? "linear" : "physical");
	lua_setfield(L, -2, "resolved_kind");
	lua_pushstring(L, text);
	lua_setfield(L, -2, "text");
	return 1;
}

static int Debugger_LuaAddressFromString(lua_State *L, const char *text)
{
	const DebuggerRegisters regs = Debugger_GetRegisters();
	uint16_t seg = 0;
	uint16_t off = 0;
	if (Debugger_LuaRegisterPair(text, regs, seg, off))
		return Debugger_LuaResolveAddress(L, seg, off);

	const char *colon = strchr(text, ':');
	if (colon == NULL)
		return luaL_error(L, "invalid address: expected segment:offset, got \"%s\"", text);

	char seg_text[32] = {};
	char off_text[32] = {};
	const size_t seg_len = static_cast<size_t>(colon - text);
	const size_t off_len = strlen(colon + 1);
	if (seg_len == 0 || seg_len >= sizeof(seg_text) || off_len == 0 || off_len >= sizeof(off_text))
		return luaL_error(L, "invalid address: expected segment:offset, got \"%s\"", text);

	memcpy(seg_text, text, seg_len);
	memcpy(off_text, colon + 1, off_len);

	if (Debugger_LuaIsAlphaString(seg_text) && Debugger_LuaIsAlphaString(off_text))
		return luaL_error(L, "invalid address: unknown register pair \"%s\"", text);

	uint32_t parsed_seg = 0;
	uint32_t parsed_off = 0;
	if (!Debugger_LuaParseHexPart(seg_text, parsed_seg) || !Debugger_LuaParseHexPart(off_text, parsed_off))
		return luaL_error(L, "invalid address: expected segment:offset, got \"%s\"", text);
	if (parsed_seg > 0xffff)
		return luaL_error(L, "invalid address: segment out of range");
	if (parsed_off > 0xffff)
		return luaL_error(L, "invalid address: offset out of range");

	return Debugger_LuaResolveAddress(L, static_cast<uint16_t>(parsed_seg), static_cast<uint16_t>(parsed_off));
}

static int Debugger_LuaAddressFromTable(lua_State *L)
{
	lua_getfield(L, 1, "seg");
	lua_getfield(L, 1, "off");
	const bool has_seg = !lua_isnil(L, -2);
	const bool has_off = !lua_isnil(L, -1);
	if (has_seg || has_off) {
		if (!has_seg || !has_off)
			return luaL_error(L, "invalid address: expected table fields seg/off or segment/offset");
		uint32_t seg = 0;
		uint32_t off = 0;
		if (!Debugger_LuaIntegerValue(L, -2, seg))
			return luaL_error(L, "invalid address: segment must be numeric");
		if (seg > 0xffff)
			return luaL_error(L, "invalid address: segment out of range");
		if (!Debugger_LuaIntegerValue(L, -1, off))
			return luaL_error(L, "invalid address: offset must be numeric");
		if (off > 0xffff)
			return luaL_error(L, "invalid address: offset out of range");
		lua_pop(L, 2);
		return Debugger_LuaResolveAddress(L, static_cast<uint16_t>(seg), static_cast<uint16_t>(off));
	}
	lua_pop(L, 2);

	lua_getfield(L, 1, "segment");
	lua_getfield(L, 1, "offset");
	const bool has_segment = !lua_isnil(L, -2);
	const bool has_offset = !lua_isnil(L, -1);
	if (!has_segment || !has_offset)
		return luaL_error(L, "invalid address: expected table fields seg/off or segment/offset");
	uint32_t seg = 0;
	uint32_t off = 0;
	if (!Debugger_LuaIntegerValue(L, -2, seg))
		return luaL_error(L, "invalid address: segment must be numeric");
	if (seg > 0xffff)
		return luaL_error(L, "invalid address: segment out of range");
	if (!Debugger_LuaIntegerValue(L, -1, off))
		return luaL_error(L, "invalid address: offset must be numeric");
	if (off > 0xffff)
		return luaL_error(L, "invalid address: offset out of range");
	lua_pop(L, 2);
	return Debugger_LuaResolveAddress(L, static_cast<uint16_t>(seg), static_cast<uint16_t>(off));
}

static void Debugger_LuaPushRegisters(lua_State *L, const DebuggerRegisters &regs, bool flags_valid = true)
{
	char addr[16] = {};
	Debugger_FormatAddress(addr, sizeof(addr), regs.cs, regs.ip);

	lua_newtable(L);

	Debugger_LuaSetIntegerField(L, "ax", Debugger_LuaLowWord(regs.eax));
	Debugger_LuaSetIntegerField(L, "bx", Debugger_LuaLowWord(regs.ebx));
	Debugger_LuaSetIntegerField(L, "cx", Debugger_LuaLowWord(regs.ecx));
	Debugger_LuaSetIntegerField(L, "dx", Debugger_LuaLowWord(regs.edx));
	Debugger_LuaSetIntegerField(L, "si", Debugger_LuaLowWord(regs.esi));
	Debugger_LuaSetIntegerField(L, "di", Debugger_LuaLowWord(regs.edi));
	Debugger_LuaSetIntegerField(L, "bp", Debugger_LuaLowWord(regs.ebp));
	Debugger_LuaSetIntegerField(L, "sp", Debugger_LuaLowWord(regs.esp));

	Debugger_LuaSetIntegerField(L, "eax", regs.eax);
	Debugger_LuaSetIntegerField(L, "ebx", regs.ebx);
	Debugger_LuaSetIntegerField(L, "ecx", regs.ecx);
	Debugger_LuaSetIntegerField(L, "edx", regs.edx);
	Debugger_LuaSetIntegerField(L, "esi", regs.esi);
	Debugger_LuaSetIntegerField(L, "edi", regs.edi);
	Debugger_LuaSetIntegerField(L, "ebp", regs.ebp);
	Debugger_LuaSetIntegerField(L, "esp", regs.esp);

	Debugger_LuaSetIntegerField(L, "cs", regs.cs);
	Debugger_LuaSetIntegerField(L, "ds", regs.ds);
	Debugger_LuaSetIntegerField(L, "es", regs.es);
	Debugger_LuaSetIntegerField(L, "fs", regs.fs);
	Debugger_LuaSetIntegerField(L, "gs", regs.gs);
	Debugger_LuaSetIntegerField(L, "ss", regs.ss);
	Debugger_LuaSetIntegerField(L, "ip", regs.ip);
	Debugger_LuaSetIntegerField(L, "eip", regs.eip);
	if (flags_valid) Debugger_LuaSetIntegerField(L, "flags", regs.flags);

	Debugger_LuaSetIntegerField(L, "ah", Debugger_LuaHighByte(regs.eax));
	Debugger_LuaSetIntegerField(L, "al", Debugger_LuaLowByte(regs.eax));
	Debugger_LuaSetIntegerField(L, "bh", Debugger_LuaHighByte(regs.ebx));
	Debugger_LuaSetIntegerField(L, "bl", Debugger_LuaLowByte(regs.ebx));
	Debugger_LuaSetIntegerField(L, "ch", Debugger_LuaHighByte(regs.ecx));
	Debugger_LuaSetIntegerField(L, "cl", Debugger_LuaLowByte(regs.ecx));
	Debugger_LuaSetIntegerField(L, "dh", Debugger_LuaHighByte(regs.edx));
	Debugger_LuaSetIntegerField(L, "dl", Debugger_LuaLowByte(regs.edx));

	if (flags_valid) {
		Debugger_LuaSetBooleanField(L, "cf", regs.flag_cf);
		Debugger_LuaSetBooleanField(L, "pf", regs.flag_pf);
		Debugger_LuaSetBooleanField(L, "af", regs.flag_af);
		Debugger_LuaSetBooleanField(L, "zf", regs.flag_zf);
		Debugger_LuaSetBooleanField(L, "sf", regs.flag_sf);
		Debugger_LuaSetBooleanField(L, "tf", regs.flag_tf);
		Debugger_LuaSetBooleanField(L, "if_", regs.flag_if);
		Debugger_LuaSetBooleanField(L, "df", regs.flag_df);
		Debugger_LuaSetBooleanField(L, "of", regs.flag_of);
	}
	Debugger_LuaSetBooleanField(L, "code_big", regs.code_big);

	lua_pushstring(L, addr);
	lua_setfield(L, -2, "addr");
}

static bool Debugger_LuaNameEquals(const char *lhs, const char *rhs)
{
	while (*lhs != '\0' && *rhs != '\0') {
		if (tolower(static_cast<unsigned char>(*lhs)) != tolower(static_cast<unsigned char>(*rhs)))
			return false;
		++lhs;
		++rhs;
	}
	return *lhs == '\0' && *rhs == '\0';
}

static bool Debugger_LuaPushRegisterValue(lua_State *L, const DebuggerRegisters &regs, const char *name)
{
	if (Debugger_LuaNameEquals(name, "ax")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.eax));
	} else if (Debugger_LuaNameEquals(name, "bx")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.ebx));
	} else if (Debugger_LuaNameEquals(name, "cx")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.ecx));
	} else if (Debugger_LuaNameEquals(name, "dx")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.edx));
	} else if (Debugger_LuaNameEquals(name, "si")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.esi));
	} else if (Debugger_LuaNameEquals(name, "di")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.edi));
	} else if (Debugger_LuaNameEquals(name, "bp")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.ebp));
	} else if (Debugger_LuaNameEquals(name, "sp")) {
		lua_pushinteger(L, Debugger_LuaLowWord(regs.esp));
	} else if (Debugger_LuaNameEquals(name, "eax")) {
		lua_pushinteger(L, regs.eax);
	} else if (Debugger_LuaNameEquals(name, "ebx")) {
		lua_pushinteger(L, regs.ebx);
	} else if (Debugger_LuaNameEquals(name, "ecx")) {
		lua_pushinteger(L, regs.ecx);
	} else if (Debugger_LuaNameEquals(name, "edx")) {
		lua_pushinteger(L, regs.edx);
	} else if (Debugger_LuaNameEquals(name, "esi")) {
		lua_pushinteger(L, regs.esi);
	} else if (Debugger_LuaNameEquals(name, "edi")) {
		lua_pushinteger(L, regs.edi);
	} else if (Debugger_LuaNameEquals(name, "ebp")) {
		lua_pushinteger(L, regs.ebp);
	} else if (Debugger_LuaNameEquals(name, "esp")) {
		lua_pushinteger(L, regs.esp);
	} else if (Debugger_LuaNameEquals(name, "cs")) {
		lua_pushinteger(L, regs.cs);
	} else if (Debugger_LuaNameEquals(name, "ds")) {
		lua_pushinteger(L, regs.ds);
	} else if (Debugger_LuaNameEquals(name, "es")) {
		lua_pushinteger(L, regs.es);
	} else if (Debugger_LuaNameEquals(name, "fs")) {
		lua_pushinteger(L, regs.fs);
	} else if (Debugger_LuaNameEquals(name, "gs")) {
		lua_pushinteger(L, regs.gs);
	} else if (Debugger_LuaNameEquals(name, "ss")) {
		lua_pushinteger(L, regs.ss);
	} else if (Debugger_LuaNameEquals(name, "ip")) {
		lua_pushinteger(L, regs.ip);
	} else if (Debugger_LuaNameEquals(name, "eip")) {
		lua_pushinteger(L, regs.eip);
	} else if (Debugger_LuaNameEquals(name, "flags")) {
		lua_pushinteger(L, regs.flags);
	} else if (Debugger_LuaNameEquals(name, "ah")) {
		lua_pushinteger(L, Debugger_LuaHighByte(regs.eax));
	} else if (Debugger_LuaNameEquals(name, "al")) {
		lua_pushinteger(L, Debugger_LuaLowByte(regs.eax));
	} else if (Debugger_LuaNameEquals(name, "bh")) {
		lua_pushinteger(L, Debugger_LuaHighByte(regs.ebx));
	} else if (Debugger_LuaNameEquals(name, "bl")) {
		lua_pushinteger(L, Debugger_LuaLowByte(regs.ebx));
	} else if (Debugger_LuaNameEquals(name, "ch")) {
		lua_pushinteger(L, Debugger_LuaHighByte(regs.ecx));
	} else if (Debugger_LuaNameEquals(name, "cl")) {
		lua_pushinteger(L, Debugger_LuaLowByte(regs.ecx));
	} else if (Debugger_LuaNameEquals(name, "dh")) {
		lua_pushinteger(L, Debugger_LuaHighByte(regs.edx));
	} else if (Debugger_LuaNameEquals(name, "dl")) {
		lua_pushinteger(L, Debugger_LuaLowByte(regs.edx));
	} else if (Debugger_LuaNameEquals(name, "cf")) {
		lua_pushboolean(L, regs.flag_cf);
	} else if (Debugger_LuaNameEquals(name, "pf")) {
		lua_pushboolean(L, regs.flag_pf);
	} else if (Debugger_LuaNameEquals(name, "af")) {
		lua_pushboolean(L, regs.flag_af);
	} else if (Debugger_LuaNameEquals(name, "zf")) {
		lua_pushboolean(L, regs.flag_zf);
	} else if (Debugger_LuaNameEquals(name, "sf")) {
		lua_pushboolean(L, regs.flag_sf);
	} else if (Debugger_LuaNameEquals(name, "tf")) {
		lua_pushboolean(L, regs.flag_tf);
	} else if (Debugger_LuaNameEquals(name, "if_")) {
		lua_pushboolean(L, regs.flag_if);
	} else if (Debugger_LuaNameEquals(name, "df")) {
		lua_pushboolean(L, regs.flag_df);
	} else if (Debugger_LuaNameEquals(name, "of")) {
		lua_pushboolean(L, regs.flag_of);
	} else if (Debugger_LuaNameEquals(name, "code_big")) {
		lua_pushboolean(L, regs.code_big);
	} else if (Debugger_LuaNameEquals(name, "addr")) {
		char addr[16] = {};
		Debugger_FormatAddress(addr, sizeof(addr), regs.cs, regs.ip);
		lua_pushstring(L, addr);
	} else {
		return false;
	}

	return true;
}

static int Debugger_LuaVersion(lua_State *L)
{
	Debugger_LuaPushVersion(L);
	return 1;
}

static void Debugger_LuaPredicateSummary(lua_State *L) {
    // Provisional local experiment only: do not change legacy status shape
    // before a consumer explicitly selects the experimental predicate surface.
    // Public capability/strict-carrier negotiation belongs to later acceptance.
    if(!getenv("DOSBOX_Y_EXPERIMENTAL_PREDICATE_GATE_G"))return;
    const auto &s=Debugger_PredicateSummary();
    lua_newtable(L);
    auto counter=[&](const char *name,uint64_t value){char text[32];snprintf(text,sizeof(text),"%llu",(unsigned long long)value);lua_pushstring(L,text);lua_setfield(L,-2,name);};
    counter("sequence",s.sequence);counter("evaluated",s.evaluated);counter("suppressed",s.suppressed);counter("propagated",s.propagated);counter("failed",s.failed);
    lua_pushboolean(L,s.overflow);lua_setfield(L,-2,"overflow");
    auto receipt=[&](const DebuggerPredicateReceipt &r){
        lua_newtable(L);counter("sequence",r.sequence);counter("id",r.id);counter("hit_count",r.hits);
        lua_pushinteger(L,r.cs);lua_setfield(L,-2,"cs");lua_pushinteger(L,r.ip);lua_setfield(L,-2,"ip");
        lua_pushliteral(L,"execute");lua_setfield(L,-2,"kind");lua_pushliteral(L,"lua");lua_setfield(L,-2,"owner");
        lua_pushstring(L,r.disposition);lua_setfield(L,-2,"disposition");lua_pushstring(L,r.diagnostic);lua_setfield(L,-2,"diagnostic");
    };
    if(s.last_valid){receipt(s.last);lua_setfield(L,-2,"last");}
    if(s.terminal_valid){receipt(s.terminal);lua_setfield(L,-2,"terminal");}
    lua_setfield(L,-2,"breakpoint_callbacks");
}

static int Debugger_LuaStatus(lua_State *L)
{
	const DebuggerStatus status = Debugger_GetStatus();

	lua_newtable(L);
	Debugger_LuaPredicateSummary(L);
	lua_pushboolean(L, status.active);
	lua_setfield(L, -2, "active");
	lua_pushboolean(L, status.run_watch);
	lua_setfield(L, -2, "run_watch");
	lua_pushboolean(L, status.run_normal);
	lua_setfield(L, -2, "run_normal");
	lua_pushboolean(L, status.stopped);
	lua_setfield(L, -2, "stopped");
	return 1;
}

static int Debugger_LuaRegs(lua_State *L)
{
	Debugger_LuaPushRegisters(L, Debugger_GetRegisters());
	return 1;
}

static int Debugger_LuaReg(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	const DebuggerRegisters regs = Debugger_GetRegisters();

	if (!Debugger_LuaPushRegisterValue(L, regs, name))
		return luaL_error(L, "unknown register: %s", name);

	return 1;
}

static bool Debugger_LuaRegisterIsFlagBit(const char *name)
{
	return Debugger_LuaNameEquals(name, "cf") || Debugger_LuaNameEquals(name, "pf") ||
	       Debugger_LuaNameEquals(name, "af") || Debugger_LuaNameEquals(name, "zf") ||
	       Debugger_LuaNameEquals(name, "sf") || Debugger_LuaNameEquals(name, "tf") ||
	       Debugger_LuaNameEquals(name, "if_") || Debugger_LuaNameEquals(name, "if") ||
	       Debugger_LuaNameEquals(name, "df") || Debugger_LuaNameEquals(name, "of");
}

static bool Debugger_LuaRegisterIsReadOnly(const char *name)
{
	return Debugger_LuaNameEquals(name, "addr") || Debugger_LuaNameEquals(name, "code_big");
}

static bool Debugger_LuaRegisterIsWritableInteger(const char *name)
{
	return Debugger_LuaNameEquals(name, "eax") || Debugger_LuaNameEquals(name, "ebx") ||
	       Debugger_LuaNameEquals(name, "ecx") || Debugger_LuaNameEquals(name, "edx") ||
	       Debugger_LuaNameEquals(name, "esi") || Debugger_LuaNameEquals(name, "edi") ||
	       Debugger_LuaNameEquals(name, "ebp") || Debugger_LuaNameEquals(name, "esp") ||
	       Debugger_LuaNameEquals(name, "eip") || Debugger_LuaNameEquals(name, "ax") ||
	       Debugger_LuaNameEquals(name, "bx") || Debugger_LuaNameEquals(name, "cx") ||
	       Debugger_LuaNameEquals(name, "dx") || Debugger_LuaNameEquals(name, "si") ||
	       Debugger_LuaNameEquals(name, "di") || Debugger_LuaNameEquals(name, "bp") ||
	       Debugger_LuaNameEquals(name, "sp") || Debugger_LuaNameEquals(name, "ip") ||
	       Debugger_LuaNameEquals(name, "al") || Debugger_LuaNameEquals(name, "ah") ||
	       Debugger_LuaNameEquals(name, "bl") || Debugger_LuaNameEquals(name, "bh") ||
	       Debugger_LuaNameEquals(name, "cl") || Debugger_LuaNameEquals(name, "ch") ||
	       Debugger_LuaNameEquals(name, "dl") || Debugger_LuaNameEquals(name, "dh") ||
	       Debugger_LuaNameEquals(name, "cs") || Debugger_LuaNameEquals(name, "ds") ||
	       Debugger_LuaNameEquals(name, "es") || Debugger_LuaNameEquals(name, "fs") ||
	       Debugger_LuaNameEquals(name, "gs") || Debugger_LuaNameEquals(name, "ss") ||
	       Debugger_LuaNameEquals(name, "flags") || Debugger_LuaNameEquals(name, "eflags");
}

static bool Debugger_LuaCheckUnsignedInteger(lua_State *L, const int index, uint64_t &value)
{
	if (lua_type(L, index) != LUA_TNUMBER)
		return false;
	int isnum = 0;
	const lua_Integer integer = lua_tointegerx(L, index, &isnum);
	if (!isnum || integer < 0)
		return false;
	value = static_cast<uint64_t>(integer);
	return true;
}

static uint64_t Debugger_LuaRegisterMaxValue(const char *name)
{
	if (Debugger_LuaNameEquals(name, "al") || Debugger_LuaNameEquals(name, "ah") ||
	    Debugger_LuaNameEquals(name, "bl") || Debugger_LuaNameEquals(name, "bh") ||
	    Debugger_LuaNameEquals(name, "cl") || Debugger_LuaNameEquals(name, "ch") ||
	    Debugger_LuaNameEquals(name, "dl") || Debugger_LuaNameEquals(name, "dh"))
		return 0xffu;
	if (Debugger_LuaNameEquals(name, "ax") || Debugger_LuaNameEquals(name, "bx") ||
	    Debugger_LuaNameEquals(name, "cx") || Debugger_LuaNameEquals(name, "dx") ||
	    Debugger_LuaNameEquals(name, "si") || Debugger_LuaNameEquals(name, "di") ||
	    Debugger_LuaNameEquals(name, "bp") || Debugger_LuaNameEquals(name, "sp") ||
	    Debugger_LuaNameEquals(name, "ip") || Debugger_LuaNameEquals(name, "cs") ||
	    Debugger_LuaNameEquals(name, "ds") || Debugger_LuaNameEquals(name, "es") ||
	    Debugger_LuaNameEquals(name, "fs") || Debugger_LuaNameEquals(name, "gs") ||
	    Debugger_LuaNameEquals(name, "ss"))
		return 0xffffu;
	return 0xffffffffu;
}

static const char *Debugger_LuaRegisterRangeText(const uint64_t max_value)
{
	if (max_value == 0xffu)
		return "0..0xff";
	if (max_value == 0xffffu)
		return "0..0xffff";
	return "0..0xffffffff";
}


namespace { static void PredicateGuard(lua_State *L);
static void PredicateRegistryGuard(lua_State *L);
static void PredicatePruneRegistrations(); static bool PredicateEnabled(); static bool PredicateManagementActive(); static bool PredicateAtCurrent(uint32_t resolved); }
static int Debugger_LuaSetReg(lua_State *L)
{
    PredicateGuard(L);
    if(Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    if(Debugger_LuaInLiveService())return luaL_error(L,"unsupported-context: mutation/guest execution in live service");
	if (lua_type(L, 1) != LUA_TSTRING)
		return luaL_error(L, "dbg.set_reg: register name must be a string");
	const char *name = lua_tostring(L, 1);

	if (Debugger_LuaRegisterIsReadOnly(name))
		return luaL_error(L, "dbg.set_reg: register '%s' is read-only", name);
	if (!Debugger_LuaRegisterIsFlagBit(name) && !Debugger_LuaRegisterIsWritableInteger(name))
		return luaL_error(L, "dbg.set_reg: unknown register '%s'", name);

	uint64_t value = 0;
	if (Debugger_LuaRegisterIsFlagBit(name)) {
		if (lua_type(L, 2) == LUA_TBOOLEAN) {
			value = lua_toboolean(L, 2) ? 1 : 0;
		} else if (!Debugger_LuaCheckUnsignedInteger(L, 2, value) || value > 1) {
			return luaL_error(L, "dbg.set_reg: value for %s must be boolean or 0/1", name);
		}
	} else {
		const uint64_t max_value = Debugger_LuaRegisterMaxValue(name);
		if (!Debugger_LuaCheckUnsignedInteger(L, 2, value) || value > max_value)
			return luaL_error(L, "dbg.set_reg: value for %s must be an integer in range %s",
			                  name,
			                  Debugger_LuaRegisterRangeText(max_value));
	}

	const DebuggerRegisterSetFailure result = Debugger_SetRegister(name, value, local_attachment);
	if (result == DebuggerRegisterSetFailure::None) {
		lua_pushboolean(L, true);
		return 1;
	}
	if (result == DebuggerRegisterSetFailure::NotStopped)
		return luaL_error(L, "dbg.set_reg: emulator is not in a debugger-safe stopped state");
	if (result == DebuggerRegisterSetFailure::InvalidValue) {
		if (Debugger_LuaRegisterIsFlagBit(name))
			return luaL_error(L, "dbg.set_reg: value for %s must be boolean or 0/1", name);
		return luaL_error(L, "dbg.set_reg: value for %s must be an integer in range %s",
		                  name,
		                  Debugger_LuaRegisterRangeText(Debugger_LuaRegisterMaxValue(name)));
	}
	return luaL_error(L, "dbg.set_reg: unknown register '%s'", name);
}

static int Debugger_LuaAddr(lua_State *L)
{
	if (lua_type(L, 1) == LUA_TSTRING)
		return Debugger_LuaAddressFromString(L, lua_tostring(L, 1));
	if (lua_istable(L, 1))
		return Debugger_LuaAddressFromTable(L);

	return luaL_error(L, "invalid address: expected segment:offset string or table");
}

static int Debugger_LuaAddrText(lua_State *L)
{
	const uint16_t seg = static_cast<uint16_t>(Debugger_LuaCheckAddressPart(L, 1, "segment", 0xffff));
	const uint16_t off = static_cast<uint16_t>(Debugger_LuaCheckAddressPart(L, 2, "offset", 0xffff));

	char text[16] = {};
	if (!Debugger_FormatAddress(text, sizeof(text), seg, off))
		return luaL_error(L, "invalid address: failed to format address");
	lua_pushstring(L, text);
	return 1;
}

static int Debugger_LuaCheckStopped(lua_State *L)
{
	const DebuggerStatus status = Debugger_GetStatus();
	if (!status.stopped)
		return luaL_error(L, "memory read requires debugger-safe stopped state");
	return 0;
}

static DebuggerAddress Debugger_LuaCheckResolvedAddress(lua_State *L)
{
	DebuggerAddress address = {};
	const int top = lua_gettop(L);
	Debugger_LuaAddr(L);

	lua_getfield(L, -1, "seg");
	address.seg = static_cast<uint16_t>(lua_tointeger(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, -1, "off");
	address.off = static_cast<uint32_t>(lua_tointeger(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, -1, "resolved");
	address.resolved = static_cast<uint64_t>(lua_tointeger(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, -1, "resolved_kind");
	const char *kind = lua_tostring(L, -1);
	address.resolved_kind = kind != NULL && strcmp(kind, "linear") == 0 ? DebuggerAddressKind::Linear : DebuggerAddressKind::Physical;
	lua_pop(L, 1);

	address.failure = DebuggerAddressFailure::None;
	address.valid = true;
	lua_settop(L, top);
	return address;
}

static uint32_t Debugger_LuaCheckMemoryLength(lua_State *L, const int index)
{
	if (lua_type(L, index) != LUA_TNUMBER)
		luaL_error(L, "invalid memory read: length must be numeric");

	int is_number = 0;
	const lua_Integer length = lua_tointegerx(L, index, &is_number);
	if (!is_number || lua_isinteger(L, index) == 0 || length < 1 ||
	    static_cast<uint64_t>(length) > DebuggerMemoryReadMax)
		luaL_error(L, "invalid memory read: length out of range");

	return static_cast<uint32_t>(length);
}

static uint32_t Debugger_LuaCheckDosStringAddressPart(lua_State *L, const int index, const char *name)
{
	if (lua_type(L, index) != LUA_TNUMBER)
		luaL_error(L, "invalid address: %s must be numeric", name);

	int is_number = 0;
	const lua_Integer integer = lua_tointegerx(L, index, &is_number);
	if (!is_number || lua_isinteger(L, index) == 0)
		luaL_error(L, "invalid address: %s must be an integer", name);
	if (integer < 0 || static_cast<uint64_t>(integer) > 0xffff)
		luaL_error(L, "invalid address: %s out of range", name);

	return static_cast<uint32_t>(integer);
}

static int Debugger_LuaMemoryReadError(lua_State *L, const DebuggerMemoryFailure failure)
{
	switch (failure) {
	case DebuggerMemoryFailure::InvalidAddress:
		return luaL_error(L, "invalid memory read: invalid address");
	case DebuggerMemoryFailure::InvalidLength:
		return luaL_error(L, "invalid memory read: length out of range");
	case DebuggerMemoryFailure::AddressOverflow:
		return luaL_error(L, "invalid memory read: address overflow");
	case DebuggerMemoryFailure::None:
		break;
	}
	return luaL_error(L, "invalid memory read");
}

static int Debugger_LuaMemRead(lua_State *L)
{
	Debugger_LuaCheckStopped(L);
	const DebuggerAddress address = Debugger_LuaCheckResolvedAddress(L);
	const uint32_t length = Debugger_LuaCheckMemoryLength(L, 2);

	uint8_t *buffer = static_cast<uint8_t *>(malloc(length));
	if (buffer == NULL)
		return luaL_error(L, "invalid memory read: failed to allocate buffer");

	uint32_t bytes_read = 0;
	const DebuggerMemoryFailure failure = Debugger_ReadMemory(address, length, buffer, length, &bytes_read);
	if (failure != DebuggerMemoryFailure::None || bytes_read != length) {
		free(buffer);
		return Debugger_LuaMemoryReadError(L, failure);
	}

	lua_pushlstring(L, reinterpret_cast<const char *>(buffer), length);
	free(buffer);
	return 1;
}

static int Debugger_LuaMemU8(lua_State *L)
{
	Debugger_LuaCheckStopped(L);
	const DebuggerAddress address = Debugger_LuaCheckResolvedAddress(L);

	uint8_t value = 0;
	uint32_t bytes_read = 0;
	const DebuggerMemoryFailure failure = Debugger_ReadMemory(address, 1, &value, 1, &bytes_read);
	if (failure != DebuggerMemoryFailure::None || bytes_read != 1)
		return Debugger_LuaMemoryReadError(L, failure);

	lua_pushinteger(L, value);
	return 1;
}

static int Debugger_LuaMemU16(lua_State *L)
{
	Debugger_LuaCheckStopped(L);
	const DebuggerAddress address = Debugger_LuaCheckResolvedAddress(L);

	uint8_t bytes[2] = {};
	uint32_t bytes_read = 0;
	const DebuggerMemoryFailure failure = Debugger_ReadMemory(address, 2, bytes, sizeof(bytes), &bytes_read);
	if (failure != DebuggerMemoryFailure::None || bytes_read != sizeof(bytes))
		return Debugger_LuaMemoryReadError(L, failure);

	const uint16_t value = static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
	lua_pushinteger(L, value);
	return 1;
}

static int Debugger_LuaPushMemoryCStr(lua_State *L, const DebuggerAddress &address, const uint32_t max_len)
{
	std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[max_len]);
	if (buffer == nullptr)
		return luaL_error(L, "invalid memory read: failed to allocate buffer");

	uint32_t bytes_read = 0;
	const DebuggerMemoryFailure failure = Debugger_ReadMemory(address, max_len, buffer.get(), max_len, &bytes_read);
	if (failure != DebuggerMemoryFailure::None || bytes_read != max_len)
		return Debugger_LuaMemoryReadError(L, failure);

	uint32_t string_len = 0;
	while (string_len < max_len && buffer[string_len] != 0)
		++string_len;

	lua_pushlstring(L, reinterpret_cast<const char *>(buffer.get()), string_len);
	return 1;
}

static int Debugger_LuaMemCStr(lua_State *L)
{
	Debugger_LuaCheckStopped(L);
	const DebuggerAddress address = Debugger_LuaCheckResolvedAddress(L);
	const uint32_t max_len = Debugger_LuaCheckMemoryLength(L, 2);
	return Debugger_LuaPushMemoryCStr(L, address, max_len);
}

static int Debugger_LuaDosString(lua_State *L)
{
	Debugger_LuaCheckStopped(L);
	const uint16_t seg = static_cast<uint16_t>(Debugger_LuaCheckDosStringAddressPart(L, 1, "segment"));
	const uint16_t off = static_cast<uint16_t>(Debugger_LuaCheckDosStringAddressPart(L, 2, "offset"));
	const uint32_t max_len = Debugger_LuaCheckMemoryLength(L, 3);

	const DebuggerAddress address = Debugger_ResolveAddress(seg, off);
	if (!address.valid)
		return luaL_error(L, "invalid address: protected-mode selector/offset is not addressable");

	return Debugger_LuaPushMemoryCStr(L, address, max_len);
}

static int Debugger_LuaCaptureDir(lua_State *L)
{
	Debugger_LuaCheckStoppedWithMessage(L, "artifact write requires debugger-safe stopped state");
	if (lua_gettop(L) == 0) {
		if (debugger_lua_capture_dir.empty())
			return luaL_error(L, "capture directory is not configured");
		lua_pushstring(L, debugger_lua_capture_dir.c_str());
		return 1;
	}
	if (lua_type(L, 1) != LUA_TSTRING)
		return luaL_error(L, "invalid capture directory: path must be a string");

	size_t length = 0;
	const char *path = lua_tolstring(L, 1, &length);
	if (length == 0)
		return luaL_error(L, "invalid capture directory: path is empty");
	if (Debugger_LuaStringHasNul(path, length))
		return luaL_error(L, "invalid capture directory: path contains NUL byte");

	std::string requested(path, length);
	if (requested[0] != '/')
		requested = Debugger_LuaJoinPath(".", requested);

	if (!Debugger_LuaEnsureDirectory(L, requested, "invalid capture directory"))
		return 0;

	struct stat st = {};
	if (lstat(requested.c_str(), &st) != 0) {
		const std::string error = Debugger_LuaHostError();
		return luaL_error(L, "invalid capture directory: failed to create directory: %s", error.c_str());
	}
	if (S_ISLNK(st.st_mode))
		return luaL_error(L, "invalid capture directory: symlink path is not allowed");
	if (!S_ISDIR(st.st_mode))
		return luaL_error(L, "invalid capture directory: path is not a directory");

	const std::string canonical = Debugger_LuaCanonicalPath(L, requested, "invalid capture directory");
#if DEBUGGER_LUA_POSIX_ARTIFACT_WRITE
	DebuggerLuaUniqueFd capture_fd(open(canonical.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
	if (!capture_fd.valid()) {
		if (errno == ELOOP)
			return luaL_error(L, "invalid capture directory: symlink path is not allowed");
		const std::string error = Debugger_LuaHostError();
		return luaL_error(L, "invalid capture directory: failed to open directory: %s", error.c_str());
	}
	struct stat canonical_st = {};
	if (fstat(capture_fd.get(), &canonical_st) != 0) {
		const std::string error = Debugger_LuaHostError();
		return luaL_error(L, "invalid capture directory: failed to open directory: %s", error.c_str());
	}
	if (!S_ISDIR(canonical_st.st_mode))
		return luaL_error(L, "invalid capture directory: path is not a directory");
	Debugger_LuaSetCaptureDirFd(capture_fd.release());
#endif
	debugger_lua_capture_dir = canonical;
	lua_pushstring(L, debugger_lua_capture_dir.c_str());
	return 1;
}

static int Debugger_LuaWriteFile(lua_State *L)
{
    if(Debugger_LuaInLiveService())return luaL_error(L,"unsupported-context: mutation/guest execution in live service");
	if (lua_type(L, 2) != LUA_TSTRING)
		return luaL_error(L, "invalid artifact data: data must be a string");

	size_t length = 0;
	const char *data = lua_tolstring(L, 2, &length);
	return Debugger_LuaWriteArtifactData(L, 1, data, length);
}


static uint32_t Debugger_LuaCheckDisasmCount(lua_State *L, const int index)
{
	if (lua_type(L, index) != LUA_TNUMBER)
		luaL_error(L, "invalid disassembly: count must be numeric");

	int is_number = 0;
	const lua_Integer count = lua_tointegerx(L, index, &is_number);
	if (!is_number || count < 1 || static_cast<uint64_t>(count) > DebuggerDisassembleMaxCount)
		luaL_error(L, "invalid disassembly: count out of range");

	return static_cast<uint32_t>(count);
}

static int Debugger_LuaDisasmError(lua_State *L, const DebuggerDisassembleFailure failure)
{
	switch (failure) {
	case DebuggerDisassembleFailure::InvalidAddress:
		return luaL_error(L, "invalid disassembly: invalid address");
	case DebuggerDisassembleFailure::InvalidCount:
		return luaL_error(L, "invalid disassembly: count out of range");
	case DebuggerDisassembleFailure::AddressOverflow:
		return luaL_error(L, "invalid disassembly: address overflow");
	case DebuggerDisassembleFailure::BufferTooSmall:
		return luaL_error(L, "invalid disassembly: output buffer too small");
	case DebuggerDisassembleFailure::InstructionSizeZero:
		return luaL_error(L, "invalid disassembly: zero-size instruction");
	case DebuggerDisassembleFailure::None:
		break;
	}
	return luaL_error(L, "invalid disassembly");
}

static int Debugger_LuaDisasm(lua_State *L)
{
	Debugger_LuaCheckStoppedWithMessage(L, "disassembly requires debugger-safe stopped state");
	const DebuggerAddress address = Debugger_LuaCheckResolvedAddress(L);
	const uint32_t count = Debugger_LuaCheckDisasmCount(L, 2);

	std::vector<DebuggerDisassemblyLine> lines(count);
	uint32_t lines_written = 0;
	const DebuggerDisassembleFailure failure = Debugger_Disassemble(address,
	                                                              count,
	                                                              lines.data(),
	                                                              count,
	                                                              &lines_written);
	if (failure != DebuggerDisassembleFailure::None || lines_written != count)
		return Debugger_LuaDisasmError(L, failure);

	lua_createtable(L, static_cast<int>(count), 0);
	for (uint32_t i = 0; i < count; ++i) {
		lua_pushstring(L, lines[i].text);
		lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
	}
	return 1;
}

static const char *Debugger_LuaBreakpointOwnerName(const DebuggerBreakpointOwner owner)
{
	switch (owner) {
	case DebuggerBreakpointOwner::Lua: return "lua";
	case DebuggerBreakpointOwner::Ncurses: return "ncurses";
	case DebuggerBreakpointOwner::Internal: return "internal";
	case DebuggerBreakpointOwner::Unknown: return "unknown";
	case DebuggerBreakpointOwner::Agent: return "agent";
	}
	return "unknown";
}

static bool Debugger_LuaBreakpointOptionKeyIsCallbackLike(const char *key)
{
	return key != NULL && (strcmp(key, "callback") == 0 || strcmp(key, "on_hit") == 0 ||
	                      strcmp(key, "handler") == 0 || strcmp(key, "function") == 0);
}

static void Debugger_LuaCheckNoFunctionOptions(lua_State *L, const int index)
{
	lua_pushnil(L);
	while (lua_next(L, index) != 0) {
		const char *key = lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : NULL;
		if (lua_type(L, -1) == LUA_TFUNCTION)
			luaL_error(L, "invalid breakpoint options: Lua callbacks/functions are not supported");
		if (Debugger_LuaBreakpointOptionKeyIsCallbackLike(key))
			luaL_error(L, "invalid breakpoint options: callback-style option '%s' is not supported", key);
		lua_pop(L, 1);
	}
}

static int Debugger_LuaBreakpointError(lua_State *L, const DebuggerBreakpointFailure failure)
{
	switch (failure) {
	case DebuggerBreakpointFailure::InvalidAddress:
		return luaL_error(L, "invalid breakpoint: invalid address");
	case DebuggerBreakpointFailure::InvalidId:
		return luaL_error(L, "invalid breakpoint: id must be a positive integer");
	case DebuggerBreakpointFailure::PermissionDenied:
		return luaL_error(L, "invalid breakpoint: permission denied");
	case DebuggerBreakpointFailure::UnsupportedType:
		return luaL_error(L, "invalid breakpoint: unsupported breakpoint type");
	case DebuggerBreakpointFailure::UnsupportedOption:
		return luaL_error(L, "invalid breakpoint: temporary breakpoints are not supported in this slice");
	case DebuggerBreakpointFailure::Duplicate:
		return luaL_error(L, "invalid breakpoint: duplicate breakpoint");
	case DebuggerBreakpointFailure::BufferTooSmall:
		return luaL_error(L, "invalid breakpoint: output buffer too small");
	case DebuggerBreakpointFailure::None:
		break;
	}
	return luaL_error(L, "invalid breakpoint");
}

static DebuggerBreakpointCreateOptions Debugger_LuaCheckBreakpointOptions(lua_State *L, std::string &label)
{
	DebuggerBreakpointCreateOptions options = {};
	options.enabled = true;
	options.temporary = false;
	options.owner = DebuggerBreakpointOwner::Lua;
	options.label = nullptr;

	if (lua_gettop(L) < 2 || lua_isnil(L, 2))
		return options;
	if (lua_type(L, 2) == LUA_TFUNCTION)
		luaL_error(L, "invalid breakpoint options: callback shorthand is not supported");
	if (!lua_istable(L, 2))
		luaL_error(L, "invalid breakpoint options: expected table");

	Debugger_LuaCheckNoFunctionOptions(L, 2);

	lua_getfield(L, 2, "enabled");
	if (!lua_isnil(L, -1)) {
		if (lua_type(L, -1) != LUA_TBOOLEAN)
			luaL_error(L, "invalid breakpoint options: enabled must be boolean");
		options.enabled = lua_toboolean(L, -1) != 0;
	}
	lua_pop(L, 1);

	lua_getfield(L, 2, "temporary");
	if (!lua_isnil(L, -1)) {
		if (lua_type(L, -1) != LUA_TBOOLEAN)
			luaL_error(L, "invalid breakpoint options: temporary must be boolean");
		if (lua_toboolean(L, -1))
			luaL_error(L, "invalid breakpoint: temporary breakpoints are not supported in this slice");
	}
	lua_pop(L, 1);

	lua_getfield(L, 2, "label");
	if (!lua_isnil(L, -1)) {
		if (lua_type(L, -1) != LUA_TSTRING)
			luaL_error(L, "invalid breakpoint options: label must be string");
		size_t length = 0;
		const char *text = lua_tolstring(L, -1, &length);
		if (Debugger_LuaStringHasNul(text, length))
			luaL_error(L, "invalid breakpoint options: label contains NUL byte");
		if (length >= DebuggerBreakpointLabelMax)
			luaL_error(L, "invalid breakpoint options: label too long");
		label.assign(text, length);
		options.label = label.c_str();
	}
	lua_pop(L, 1);

	lua_pushnil(L);
	while (lua_next(L, 2) != 0) {
		if (lua_type(L, -2) != LUA_TSTRING)
			luaL_error(L, "invalid breakpoint options: option keys must be strings");
		const char *key = lua_tostring(L, -2);
		if (strcmp(key, "enabled") != 0 && strcmp(key, "temporary") != 0 && strcmp(key, "label") != 0)
			luaL_error(L, "invalid breakpoint options: unknown option '%s'", key);
		lua_pop(L, 1);
	}

	return options;
}

static lua_Integer Debugger_LuaCheckBreakpointId(lua_State *L, const int index)
{
	int is_number = 0;
	const lua_Integer id = lua_tointegerx(L, index, &is_number);
	if (!is_number || id <= 0)
		luaL_error(L, "invalid breakpoint: id must be a positive integer");
	return id;
}

static int Debugger_LuaBpSet(lua_State *L)
{
    PredicateRegistryGuard(L);
    if(!PredicateManagementActive())PredicateGuard(L);
    if(!PredicateManagementActive() && Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    if(!PredicateManagementActive() && Debugger_LuaInLiveService() && !(getenv("DOSBOX_Y_EXPERIMENTAL_PREDICATE_GATE_G") && Debugger_LiveOwnsStop(DOSBOX_MachineServiceOwner().token)))return luaL_error(L,"unsupported-context: mutation/guest execution in live service");
	Debugger_LuaCheckStoppedWithMessage(L, "breakpoint API requires debugger-safe stopped state");
	const DebuggerAddress address = Debugger_LuaCheckResolvedAddress(L);
    if(PredicateAtCurrent(address.resolved))return luaL_error(L,"cannot create breakpoint at current predicate location");
	std::string label;
	const DebuggerBreakpointCreateOptions options = Debugger_LuaCheckBreakpointOptions(L, label);

	uint64_t id = 0;
	const DebuggerBreakpointFailure failure = Debugger_BreakpointSet(address, options, &id, local_attachment);
	if (failure != DebuggerBreakpointFailure::None)
		return Debugger_LuaBreakpointError(L, failure);
	lua_pushinteger(L, static_cast<lua_Integer>(id));
	return 1;
}

static int Debugger_LuaBpClear(lua_State *L)
{
    const bool own=PredicateManagementActive();
    if(!own)PredicateGuard(L);
    if(!own && Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    if(!own && Debugger_LuaInLiveService() && !(getenv("DOSBOX_Y_EXPERIMENTAL_PREDICATE_GATE_G") && Debugger_LiveOwnsStop(DOSBOX_MachineServiceOwner().token)))return luaL_error(L,"unsupported-context: mutation/guest execution in live service");
	Debugger_LuaCheckStoppedWithMessage(L, "breakpoint API requires debugger-safe stopped state");
	const lua_Integer id = Debugger_LuaCheckBreakpointId(L, 1);
	bool cleared = false;
	const DebuggerBreakpointFailure failure = Debugger_BreakpointClear(static_cast<uint64_t>(id), DebuggerBreakpointOwner::Lua, &cleared, local_attachment);
	if (failure == DebuggerBreakpointFailure::InvalidId)
		return Debugger_LuaBreakpointError(L, failure);
	if (failure != DebuggerBreakpointFailure::None && failure != DebuggerBreakpointFailure::PermissionDenied)
		return Debugger_LuaBreakpointError(L, failure);
    PredicatePruneRegistrations();
	lua_pushboolean(L, cleared);
	return 1;
}

static void Debugger_LuaPushBreakpointInfo(lua_State *L, const DebuggerBreakpointInfo &info)
{
	char text[16] = {};
	Debugger_FormatAddress(text, sizeof(text), info.seg, info.off);

	lua_newtable(L);
	Debugger_LuaSetIntegerField(L, "id", static_cast<lua_Integer>(info.id));
	lua_pushstring(L, text);
	lua_setfield(L, -2, "addr");
	lua_pushstring(L, text);
	lua_setfield(L, -2, "text");
	Debugger_LuaSetIntegerField(L, "seg", info.seg);
	Debugger_LuaSetIntegerField(L, "off", info.off);
	Debugger_LuaSetIntegerField(L, "resolved", static_cast<lua_Integer>(info.resolved));
	lua_pushstring(L, info.resolved_kind == DebuggerAddressKind::Linear ? "linear" : "physical");
	lua_setfield(L, -2, "resolved_kind");
	Debugger_LuaSetBooleanField(L, "enabled", info.enabled);
	Debugger_LuaSetBooleanField(L, "temporary", info.temporary);
	lua_pushstring(L, Debugger_LuaBreakpointOwnerName(info.owner));
	lua_setfield(L, -2, "owner");
	Debugger_LuaSetIntegerField(L, "hit_count", static_cast<lua_Integer>(info.hit_count));
	if (info.label[0] != '\0') {
		lua_pushstring(L, info.label);
		lua_setfield(L, -2, "label");
	}
}

static int Debugger_LuaBpList(lua_State *L)
{
	Debugger_LuaCheckStoppedWithMessage(L, "breakpoint API requires debugger-safe stopped state");
	uint32_t count = 0;
	DebuggerBreakpointFailure failure = Debugger_BreakpointList(nullptr, 0, &count, false);
	if (failure != DebuggerBreakpointFailure::None)
		return Debugger_LuaBreakpointError(L, failure);

	std::vector<DebuggerBreakpointInfo> items(count);
	failure = Debugger_BreakpointList(items.data(), count, &count, false);
	if (failure != DebuggerBreakpointFailure::None)
		return Debugger_LuaBreakpointError(L, failure);

	lua_createtable(L, static_cast<int>(count), 0);
	for (uint32_t i = 0; i < count; ++i) {
		Debugger_LuaPushBreakpointInfo(L, items[i]);
		lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
	}
	return 1;
}

static int Debugger_LuaBpClearAll(lua_State *L)
{
    if(!PredicateManagementActive())PredicateGuard(L);
    if(!PredicateManagementActive() && Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    if(!PredicateManagementActive() && Debugger_LuaInLiveService())return luaL_error(L,"unsupported-context: mutation/guest execution in live service");
	Debugger_LuaCheckStoppedWithMessage(L, "breakpoint API requires debugger-safe stopped state");
	uint32_t cleared = 0;
	const DebuggerBreakpointFailure failure = Debugger_BreakpointClearOwned(DebuggerBreakpointOwner::Lua, &cleared, local_attachment);
	if (failure != DebuggerBreakpointFailure::None)
		return Debugger_LuaBreakpointError(L, failure);
    PredicatePruneRegistrations();
	lua_pushboolean(L, cleared != 0);
	return 1;
}


static const char *Debugger_LuaStopTypeName(const DebuggerStopType type)
{
	switch (type) {
	case DebuggerStopType::Step: return "step";
	case DebuggerStopType::Breakpoint: return "breakpoint";
	case DebuggerStopType::Timeout: return "timeout";
	case DebuggerStopType::Exit: return "exit";
	case DebuggerStopType::ManualStop: return "manual_stop";
	case DebuggerStopType::Error: return "error";
	case DebuggerStopType::None: break;
	}
	return "error";
}

static uint32_t Debugger_LuaCheckStepCount(lua_State *L)
{
	if (lua_gettop(L) == 0 || lua_isnil(L, 1))
		return 1;
	if (lua_type(L, 1) != LUA_TNUMBER)
		luaL_error(L, "dbg.step: count must be an integer");

	int is_number = 0;
	const lua_Integer count = lua_tointegerx(L, 1, &is_number);
	if (!is_number || lua_isinteger(L, 1) == 0)
		luaL_error(L, "dbg.step: count must be an integer");
	if (count < 1 || count > 10000)
		luaL_error(L, "dbg.step: count must be between 1 and 10000");
	return static_cast<uint32_t>(count);
}

static bool Debugger_LuaRunUntilOptionKeyIsCallbackLike(const char *key)
{
	return key != NULL && (strcmp(key, "callback") == 0 || strcmp(key, "on_event") == 0 ||
	                      strcmp(key, "on_hit") == 0 || strcmp(key, "handler") == 0 ||
	                      strcmp(key, "filter") == 0 || strcmp(key, "predicate") == 0 ||
	                      strcmp(key, "function") == 0);
}

static void Debugger_LuaCheckRunUntilDataOnly(lua_State *L,
                                              const int index,
                                              std::set<const void *> &seen_tables,
                                              const char *api_name)
{
	const int absolute_index = lua_absindex(L, index);
	if (lua_type(L, absolute_index) != LUA_TTABLE)
		return;

	const void *table_pointer = lua_topointer(L, absolute_index);
	if (seen_tables.count(table_pointer) != 0)
		return;
	seen_tables.insert(table_pointer);

	lua_pushnil(L);
	while (lua_next(L, absolute_index) != 0) {
		const char *key = lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : NULL;
		if (lua_type(L, -2) == LUA_TFUNCTION || lua_type(L, -1) == LUA_TFUNCTION)
			luaL_error(L, "%s: Lua callbacks/functions are not supported", api_name);
		if (Debugger_LuaRunUntilOptionKeyIsCallbackLike(key))
			luaL_error(L, "%s: unsupported callback-style option '%s'", api_name, key);
		if (lua_type(L, -1) == LUA_TTABLE)
			Debugger_LuaCheckRunUntilDataOnly(L, -1, seen_tables, api_name);
		lua_pop(L, 1);
	}
}

static uint32_t Debugger_LuaCheckRunUntilOptions(lua_State *L, const int index, const char *api_name)
{
	static constexpr uint32_t DefaultInstructionLimit = 100000;
	static constexpr uint32_t MaxInstructionLimit = 1000000;
	const int absolute_index = lua_absindex(L, index);

	if (lua_gettop(L) < absolute_index || lua_isnil(L, absolute_index))
		return DefaultInstructionLimit;
	if (!lua_istable(L, absolute_index))
		luaL_error(L, "%s: options must be a table", api_name);

	std::set<const void *> seen_tables;
	Debugger_LuaCheckRunUntilDataOnly(L, absolute_index, seen_tables, api_name);

	uint32_t instruction_limit = DefaultInstructionLimit;
	lua_getfield(L, absolute_index, "instruction_limit");
	if (!lua_isnil(L, -1)) {
		if (lua_type(L, -1) != LUA_TNUMBER)
			luaL_error(L, "%s: instruction_limit must be an integer", api_name);
		int is_number = 0;
		const lua_Integer value = lua_tointegerx(L, -1, &is_number);
		if (!is_number || lua_isinteger(L, -1) == 0)
			luaL_error(L, "%s: instruction_limit must be an integer", api_name);
		if (value < 1 || value > MaxInstructionLimit)
			luaL_error(L, "%s: instruction_limit must be between 1 and 1000000", api_name);
		instruction_limit = static_cast<uint32_t>(value);
	}
	lua_pop(L, 1);

	for (const char *reserved_key : {"stop_on_exit", "stop_on_error"}) {
		lua_getfield(L, absolute_index, reserved_key);
		if (!lua_isnil(L, -1)) {
			if (lua_type(L, -1) != LUA_TBOOLEAN)
				luaL_error(L, "%s: %s must be true", api_name, reserved_key);
			if (lua_toboolean(L, -1) == 0)
				luaL_error(L, "%s: %s=false is not supported in this version", api_name, reserved_key);
		}
		lua_pop(L, 1);
	}

	lua_pushnil(L);
	while (lua_next(L, absolute_index) != 0) {
		if (lua_type(L, -2) != LUA_TSTRING)
			luaL_error(L, "%s: option keys must be strings", api_name);
		const char *key = lua_tostring(L, -2);
		if (strcmp(key, "instruction_limit") != 0 && strcmp(key, "stop_on_exit") != 0 && strcmp(key, "stop_on_error") != 0)
			luaL_error(L, "%s: unsupported option '%s'", api_name, key);
		lua_pop(L, 1);
	}

	return instruction_limit;
}

static void Debugger_LuaPushStopEvent(lua_State *L, const DebuggerStopEvent &event)
{
	char addr[16] = {};
	Debugger_FormatAddress(addr, sizeof(addr), event.cs, event.ip);

	lua_newtable(L);
	lua_pushstring(L, Debugger_LuaStopTypeName(event.type));
	lua_setfield(L, -2, "type");
	Debugger_LuaSetIntegerField(L, "seq", static_cast<lua_Integer>(event.sequence));
	Debugger_LuaSetIntegerField(L, "instruction_count", static_cast<lua_Integer>(event.instruction_count));
	Debugger_LuaSetIntegerField(L, "requested_count", static_cast<lua_Integer>(event.requested_count));
	Debugger_LuaSetIntegerField(L, "cs", event.cs);
	Debugger_LuaSetIntegerField(L, "ip", event.ip);
	lua_pushstring(L, addr);
	lua_setfield(L, -2, "addr");
	Debugger_LuaPushRegisters(L, Debugger_GetRegisters());
	lua_setfield(L, -2, "regs");

	if (event.type == DebuggerStopType::Breakpoint) {
		Debugger_LuaSetIntegerField(L, "bp_id", static_cast<lua_Integer>(event.breakpoint_id));
		lua_pushstring(L, Debugger_LuaBreakpointOwnerName(event.breakpoint_owner));
		lua_setfield(L, -2, "bp_owner");
		Debugger_LuaSetIntegerField(L, "bp_hit_count", static_cast<lua_Integer>(event.breakpoint_hit_count));
		if (event.breakpoint_label[0] != '\0') {
			lua_pushstring(L, event.breakpoint_label);
			lua_setfield(L, -2, "bp_label");
		}
	}
	if ((event.type == DebuggerStopType::Timeout || event.type == DebuggerStopType::ManualStop || event.type == DebuggerStopType::Error) && event.message[0] != '\0') {
		lua_pushstring(L, event.message);
		lua_setfield(L, -2, "message");
	}
}

// Standalone file evaluation owns one provisional intent. No decoder entry
// occurs until the file and lua_close (including finalizers) have unwound.
static bool local_evaluation=false, local_intent=false;
static bool local_closing=false;
static NativeDebug::Request local_request;
// Results survive file evaluation; the session outlives the ephemeral lua_State.
static int Debugger_LuaRequestStep(lua_State *L)
{
    if (!local_evaluation || local_closing)
        return luaL_error(L,"request_step requires standalone file evaluation");
    if (local_intent) return luaL_error(L,"one execution intent per evaluation");
    const auto count=luaL_optinteger(L,1,1);
    if (count<1 || count>10000) return luaL_error(L,"CPU cycle budget must be 1..10000");
    local_request={};
    local_request.retain_result=true;
    local_request.frontend=NativeDebug::Frontend::Interactive;
    local_request.expected=DEBUG_NativeExecutionContext();
    local_request.cycle_budget=uint32_t(count);
    if (!local_request.expected.invocation)
        return luaL_error(L,"unsafe native execution context");
    if (!local_attachment.serial)
        local_attachment=DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0);
    local_request.attachment=local_attachment;
    if (!DEBUG_AdmitNativeEffect(local_attachment))
        return luaL_error(L,"native execution admission refused");
    local_intent=true;
    lua_pushboolean(L,true);
    return 1;
}
static int Debugger_LuaExecutionResult(lua_State *L)
{
    const auto *result=DEBUG_NativeExecutionResult(local_attachment);
    if (!result) {lua_pushnil(L);return 1;}
    lua_newtable(L);
    Debugger_LuaSetIntegerField(L,"operation",result->operation);
    Debugger_LuaSetIntegerField(L,"decoder_calls",result->decoder_calls);
    Debugger_LuaSetIntegerField(L,"cs",result->cs);
    Debugger_LuaSetIntegerField(L,"ip",result->ip);
    lua_pushboolean(L,result->failed);lua_setfield(L,-2,"failed");
    lua_pushboolean(L,result->budget_exhausted);lua_setfield(L,-2,"budget_exhausted");
    return 1;
}
static int Debugger_LuaReleaseExecution(lua_State *L)
{
    const auto operation=luaL_checkinteger(L,1);
    lua_pushboolean(L,operation>0 && DEBUG_ReleaseNativeExecutionResult(
        local_attachment,uint64_t(operation)));
    return 1;
}

static int Debugger_LuaStep(lua_State *L)
{
    PredicateGuard(L);
    if(PredicateEnabled())return luaL_error(L,"synchronous guest execution requires predicate detachment");
    if(Debugger_LuaInLiveService())return luaL_error(L,"unsupported-context: mutation/guest execution in live service");
	const uint32_t count = Debugger_LuaCheckStepCount(L);
	Debugger_LuaCheckStoppedWithMessage(L, "dbg.step: emulator is not in a debugger-safe stopped state");
    if (local_intent || local_closing) return luaL_error(L,"execution intent or finalizer cannot execute synchronously");
    // Preserve legacy callback-poison reporting as a native Error event rather
    // than converting it into a Lua exception. This branch cannot execute.
    if (DEBUG_ContinuationUnavailable()) {
        Debugger_LuaPushStopEvent(L, Debugger_Step(count));
        return 1;
    }
    if (!DEBUG_AdmitNativeEffect(local_attachment))
        return luaL_error(L,"native execution admission refused");
	const DebuggerStopEvent event = Debugger_Step(count);
	Debugger_LuaPushStopEvent(L, event);
	return 1;
}

static int Debugger_LuaRunUntilEvent(lua_State *L)
{
    PredicateGuard(L);
    if(PredicateEnabled())return luaL_error(L,"synchronous guest execution requires predicate detachment");
    if(Debugger_LuaInLiveService())return luaL_error(L,"unsupported-context: mutation/guest execution in live service");
	const uint32_t instruction_limit = Debugger_LuaCheckRunUntilOptions(L, 1, "dbg.run_until_event");
	Debugger_LuaCheckStoppedWithMessage(L, "dbg.run_until_event: emulator is not in a debugger-safe stopped state");
    if (local_intent || local_closing) return luaL_error(L,"execution intent or finalizer cannot execute synchronously");
    if (DEBUG_ContinuationUnavailable()) {
        Debugger_LuaPushStopEvent(L, Debugger_RunUntilEvent(instruction_limit));
        return 1;
    }
    if (!DEBUG_AdmitNativeEffect(local_attachment))
        return luaL_error(L,"native execution admission refused");
	const DebuggerStopEvent event = Debugger_RunUntilEvent(instruction_limit);
	Debugger_LuaPushStopEvent(L, event);
	return 1;
}

static int Debugger_LuaLog(lua_State *L)
{
	const char *message = luaL_checkstring(L, 1);
	if (interactive_lua && lua_topointer(L, LUA_REGISTRYINDEX) ==
	                       lua_topointer(interactive_lua, LUA_REGISTRYINDEX))
		DEBUG_ShowMsg("Lua debugger: %s", message);
	else
		LOG_MSG("Lua debugger: %s", message);
	return 0;
}

static int Debugger_LuaLogf(lua_State *L)
{
	luaL_checkstring(L, 1);

	lua_getglobal(L, "string");
	lua_getfield(L, -1, "format");
	lua_remove(L, -2);
	lua_insert(L, 1);

	if (lua_pcall(L, lua_gettop(L) - 1, 1, 0) != LUA_OK)
		return lua_error(L);

	const char *message = luaL_checkstring(L, -1);
	if (interactive_lua && lua_topointer(L, LUA_REGISTRYINDEX) ==
	                       lua_topointer(interactive_lua, LUA_REGISTRYINDEX))
		DEBUG_ShowMsg("Lua debugger: %s", message);
	else
		LOG_MSG("Lua debugger: %s", message);
	return 0;
}

namespace {
struct PredicateRegistration {uint64_t id;int ref;};
static std::vector<PredicateRegistration> predicates;
static lua_State *predicate_state=nullptr;
static int predicate_ref=LUA_NOREF;
static bool predicate_budget_failed=false;
static uint64_t predicate_id=0,predicate_hits=0;
static uint16_t predicate_cs=0;
static uint32_t predicate_ip=0;
static bool predicate_active=false;
static bool predicate_closing=false;
static bool PredicateEnabled() {return !predicates.empty();}
// Active native receipt must keep its registry reference until protected return.
// Ordinary clear at an inert stop must not depend on a future callback to prune.
static void PredicatePruneRegistrations() {
    if(predicate_active || !predicate_state)return;
    uint32_t n=0;Debugger_BreakpointList(nullptr,0,&n,true);
    std::vector<DebuggerBreakpointInfo> remaining(n);
    Debugger_BreakpointList(remaining.data(),n,&n,true);
    for(auto it=predicates.begin();it!=predicates.end();) {
        bool exists=false;for(const auto &bp:remaining)if(bp.id==it->id)exists=true;
        if(!exists){luaL_unref(predicate_state,LUA_REGISTRYINDEX,it->ref);it=predicates.erase(it);}
        else ++it;
    }
    predicate_ref=LUA_NOREF;
}
// No Lua invocation/GC owns native teardown. Called only after protected return.
static void PredicateReleaseRegistrations() {
    assert(!predicate_active);
    for(const auto &p:predicates) {
        bool cleared=false;Debugger_BreakpointClear(p.id,DebuggerBreakpointOwner::Lua,&cleared,local_attachment);
        if(predicate_state)luaL_unref(predicate_state,LUA_REGISTRYINDEX,p.ref);
    }
    predicates.clear();predicate_ref=LUA_NOREF;
}

static void PredicateGuard(lua_State *L) {
    if (DEBUG_NativeExecutionInFlight() || !NativeControl::diagnosticEffectAllowed())luaL_error(L,"native execution or exclusive control lease in flight");
    if(predicate_active || predicate_closing)luaL_error(L,"guest effects forbidden during breakpoint predicate");
}
// Wrap library loaders at state initialization so aliases and coroutine threads
// retain the same debugger context check, including error/close unwinding.
static int PredicateLoad(lua_State *L) {
    PredicateGuard(L);
    const int nargs=lua_gettop(L);
    lua_pushvalue(L,lua_upvalueindex(1));lua_insert(L,1);
    lua_call(L,nargs,LUA_MULTRET);
    return lua_gettop(L);
}
static void PredicateWrapField(lua_State *L,int table,const char *name) {
    table=lua_absindex(L,table);
    lua_getfield(L,table,name);
    if(lua_isfunction(L,-1)) {
        lua_pushcclosure(L,PredicateLoad,1);lua_setfield(L,table,name);
    } else lua_pop(L,1);
}
static void PredicateInstallLoadGuards(lua_State *L) {
    lua_pushglobaltable(L);
    for(const char *name:{"load","loadfile","dofile","require"})PredicateWrapField(L,-1,name);
    lua_pop(L,1);
    lua_getglobal(L,"package");
    if(lua_istable(L,-1)) {
        PredicateWrapField(L,-1,"loadlib");
        lua_getfield(L,-1,"searchers");
        if(lua_istable(L,-1))for(lua_Integer i=1;i<=lua_Integer(lua_rawlen(L,-1));++i) {
            lua_rawgeti(L,-1,i);
            if(lua_isfunction(L,-1)){lua_pushcclosure(L,PredicateLoad,1);lua_rawseti(L,-2,i);}
            else lua_pop(L,1);
        }
        lua_pop(L,1);
    }
    lua_pop(L,1);
}
static void PredicateRegistryGuard(lua_State *L) {
    if(predicate_closing)luaL_error(L,"breakpoint management unavailable during Lua close");
}
static bool PredicateManagementActive() {return predicate_active && !predicate_closing;}
static bool PredicateAtCurrent(uint32_t resolved) {
    return predicate_active && resolved==Debugger_ResolveAddress(predicate_cs,predicate_ip).resolved;
}
static int PredicateAttach(lua_State *L) {
    if(predicate_closing)return luaL_error(L,"registration unavailable during Lua close");
    const bool detach=lua_isnil(L,2);
    if(!predicate_active && Debugger_PredicateHeld())
        return luaL_error(L,"registration replacement unavailable during predicate hold");
    lua_rawgeti(L,LUA_REGISTRYINDEX,LUA_RIDX_MAINTHREAD);
    const bool same_state=predicate_state && lua_tothread(L,-1)==predicate_state;
    lua_pop(L,1);
    if(!same_state)return luaL_error(L,"persistent registration unavailable in legacy one-shot or foreign Lua state");
    const auto id=uint64_t(luaL_checkinteger(L,1));
    uint32_t count=0;
    Debugger_BreakpointList(nullptr,0,&count,true);
    std::vector<DebuggerBreakpointInfo> items(count);
    Debugger_BreakpointList(items.data(),count,&count,true);
    bool owned=false;
    for(const auto &item:items)if(item.id==id && item.owner==DebuggerBreakpointOwner::Lua) {
        owned=true;
        if(!detach && ((id==predicate_id && predicate_active) || PredicateAtCurrent(item.resolved)))
            return luaL_error(L,"cannot replace callback at current predicate location");
        const auto &target=Debugger_ExecutionTarget();
        if(!lua_isnil(L,2) && target.breakpoint_id && !strcmp(target.reason,"armed") &&
           item.resolved==Debugger_ResolveAddress(target.cs,target.ip).resolved)
            return luaL_error(L,"predicate collides with armed execution target");
    }
    if(!owned && !(predicate_active && detach && id==predicate_id))return luaL_error(L,"predicate requires existing Lua-owned breakpoint");
    if(lua_gettop(L)<2 || lua_gettop(L)>3)return luaL_error(L,"on_break requires id, function or nil, and optional once options");
    bool once=false;
    if(lua_gettop(L)==3) {
        luaL_checktype(L,3,LUA_TTABLE);
        lua_pushnil(L);
        while(lua_next(L,3)) {
            if(lua_type(L,-2)!=LUA_TSTRING || strcmp(lua_tostring(L,-2),"once"))
                return luaL_error(L,"unknown on_break option");
            luaL_checktype(L,-1,LUA_TBOOLEAN);
            once=lua_toboolean(L,-1);lua_pop(L,1);
        }
        if(detach)return luaL_error(L,"detach does not accept options");
    }
    // Validate everything before mutating either native lifetime or registry.
    if(!detach)luaL_checktype(L,2,LUA_TFUNCTION);
    if(once && !Debugger_BreakpointOnce(id,DebuggerBreakpointOwner::Lua,local_attachment))
        return luaL_error(L,"cannot mark breakpoint once");
    if(!lua_isnil(L,2))luaL_checktype(L,2,LUA_TFUNCTION);
    for(auto it=predicates.begin();it!=predicates.end();++it)if(it->id==id) {
        luaL_unref(L,LUA_REGISTRYINDEX,it->ref);predicates.erase(it);break;
    }
    if(!lua_isnil(L,2)) {lua_pushvalue(L,2);predicates.push_back({id,luaL_ref(L,LUA_REGISTRYINDEX)});}
    return 0;
}
}
bool Debugger_LuaPredicateSelect(uint64_t id,uint16_t cs,uint32_t ip,uint64_t hits) {
    for(const auto &p:predicates)if(p.id==id) {
        predicate_ref=p.ref;predicate_id=id;predicate_cs=cs;predicate_ip=ip;predicate_hits=hits;return true;
    }
    return false;
}

static void Debugger_LuaRegisterDbg(lua_State *L)
{
	lua_newtable(L);
	lua_pushcfunction(L, Debugger_LuaVersion);
	lua_setfield(L, -2, "version");
	lua_pushcfunction(L, Debugger_LuaStatus);
	lua_setfield(L, -2, "status");
	lua_pushcfunction(L, Debugger_LuaRegs);
	lua_setfield(L, -2, "regs");
	lua_pushcfunction(L, Debugger_LuaReg);
	lua_setfield(L, -2, "reg");
	lua_pushcfunction(L, Debugger_LuaSetReg);
	lua_setfield(L, -2, "set_reg");
	lua_pushcfunction(L, Debugger_LuaAddr);
	lua_setfield(L, -2, "addr");
	lua_pushcfunction(L, Debugger_LuaAddrText);
	lua_setfield(L, -2, "addr_text");
	lua_pushcfunction(L, Debugger_LuaMemRead);
	lua_setfield(L, -2, "mem_read");
	lua_pushcfunction(L, Debugger_LuaMemU8);
	lua_setfield(L, -2, "mem_u8");
	lua_pushcfunction(L, Debugger_LuaMemU16);
	lua_setfield(L, -2, "mem_u16");
	lua_pushcfunction(L, Debugger_LuaMemCStr);
	lua_setfield(L, -2, "mem_cstr");
	lua_pushcfunction(L, Debugger_LuaDosString);
	lua_setfield(L, -2, "dos_string");
	lua_pushcfunction(L, Debugger_LuaCaptureDir);
	lua_setfield(L, -2, "capture_dir");
	lua_pushcfunction(L, Debugger_LuaWriteFile);
	lua_setfield(L, -2, "write_file");
	lua_pushcfunction(L, Debugger_LuaDisasm);
	lua_setfield(L, -2, "disasm");
	lua_pushcfunction(L, Debugger_LuaBpSet);
	lua_setfield(L, -2, "bp_set");
	lua_pushcfunction(L, Debugger_LuaBpClear);
	lua_setfield(L, -2, "bp_clear");
	lua_pushcfunction(L, Debugger_LuaBpList);
	lua_setfield(L, -2, "bp_list");
	lua_pushcfunction(L, Debugger_LuaBpClearAll);
	lua_setfield(L, -2, "bp_clear_all");
	lua_pushcfunction(L, Debugger_LuaRequestStep);
    lua_setfield(L,-2,"request_step");
    lua_pushcfunction(L, Debugger_LuaExecutionResult);
    lua_setfield(L,-2,"execution_result");
    lua_pushcfunction(L, Debugger_LuaReleaseExecution);
    lua_setfield(L,-2,"release_execution");
	lua_pushcfunction(L, Debugger_LuaStep);
	lua_setfield(L, -2, "step");
	lua_pushcfunction(L, Debugger_LuaRunUntilEvent);
	lua_setfield(L, -2, "run_until_event");
	lua_pushcfunction(L, Debugger_LuaLog);
	lua_setfield(L, -2, "log");
	lua_pushcfunction(L, Debugger_LuaLogf);
	lua_setfield(L, -2, "logf");
    if(getenv("DOSBOX_Y_EXPERIMENTAL_PREDICATE_GATE_G")) {
        lua_pushcfunction(L,PredicateAttach);lua_setfield(L,-2,"on_break");
    }
	lua_setglobal(L, "dbg");
}


namespace {
std::unique_ptr<NativeInput::LiveTransport> live_transport;
NativeInput::StateRoot live_state_root;
lua_State *live_lua=nullptr;
bool persistent_local=false,service_installed=false,close_pending=false;
bool lean_protocol_closing=false;
std::string local_load_path;
void LiveSlice(void *);
void EnsureDebuggerService() {
    if(!service_installed){DOSBOX_SetMachineService(LiveSlice,nullptr);service_installed=true;}
}
std::string live_line;
const char *live_module=nullptr;
bool live_checked=false,live_failed=false,live_consumed=false,live_active=false,live_have_line=false,live_channel_reported=false;
uint64_t live_slices=0,live_start=0,live_end=0;
uint64_t LiveNow() {return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();}
// Process-local captures cannot reconstruct the suspended native EXEC stack.
// Bytes may be replaced by id; successful restores still require disposal
// before their original EXEC owner returns.
struct LiveCheckpointImage {
    SaveState::Checkpoint parts;
    uint64_t owner=0;
    uint16_t psp=0;
    std::string program,mounts;
};
std::map<std::string,LiveCheckpointImage> live_checkpoints;
uint64_t checkpoint_owner=0,checkpoint_epoch=0;
bool checkpoint_restored=false;
std::string CheckpointMounts() {
    std::string result;
    for (unsigned i=0;i<DOS_DRIVES;i++) if (Drives[i]) {
        const char *info=Drives[i]->GetInfo();
        if (strcmp(info,"Internal Virtual Drive") &&
            (strncmp(info,"local directory ",16) || typeid(*Drives[i])!=typeid(localDrive)))
            return {};
        // Native directory enumeration cursors are not encoded by the DOS
        // owner. A mount being read-only does not make those cursors safe.
        if (typeid(*Drives[i])==typeid(localDrive) &&
            static_cast<localDrive *>(Drives[i])->StateSearchActive()) {LOG_MSG("Checkpoint active search on drive %u",i); return {}; }
        result+=std::to_string(i)+":"+info+"\n";
    }
    // Original DOS owner reopens local files without truncating host content.
    // Other external native handle kinds have no admitted reconstruction here.
    for (unsigned i=0;i<DOS_FILES;i++)
        if (Files[i] && Files[i]->GetDrive()<DOS_DRIVES &&
            typeid(*Files[i])!=typeid(LocalFile)) return {};
    return result;
}
static bool CheckpointInstrumentationAllowed(bool load,DebuggerBreakpointFailure error,
        const DebuggerBreakpointInfo *points,uint32_t count) {
    const auto &target=Debugger_ExecutionTarget();
    if(error!=DebuggerBreakpointFailure::None || count>256)return false;
    // breakpoint_id is immutable receipt identity, not proof of a retained BP.
    // The complete native inventory above is authoritative for quiescence.
    // Do not admit an active target or adopt another machine owner's history.
    if(!load)return !count && !PredicateEnabled() &&
        (!target.breakpoint_id || (target.owner==DOSBOX_MachineServiceOwner().token &&
                                  strcmp(target.reason,"armed")));
    for(uint32_t i=0;i<count;++i)
        if(points[i].owner!=DebuggerBreakpointOwner::Lua &&
           !(points[i].owner==DebuggerBreakpointOwner::Internal && points[i].id==target.breakpoint_id && target.owner==DOSBOX_MachineServiceOwner().token &&
             !strcmp(target.reason,"armed")))return false;
    return true;
}
void LiveFields(lua_State *L,int table,const char *const *allowed);
const char *LiveString(lua_State *L,int index);
void LiveCheckpointArguments(lua_State *L) {
    if (lua_gettop(L)!=1 || !lua_istable(L,1))luaL_error(L,"invalid checkpoint request");
    if(lua_getmetatable(L,1))luaL_error(L,"checkpoint requires plain data");
    static const char *fields[]={"op","id",nullptr};
    LiveFields(L,1,fields);
    // Validate both values before constructing any resource-owning C++ value.
    for(const char *name:{"op","id"}) {
        lua_getfield(L,1,name);LiveString(L,-1);lua_pop(L,1);
    }
    lua_getfield(L,1,"op");const char *op=LiveString(L,-1);
    const bool valid_op=!strcmp(op,"save") || !strcmp(op,"load");lua_pop(L,1);
    lua_getfield(L,1,"id");const char *id=LiveString(L,-1);
    const size_t length=strlen(id);
    if(!valid_op || !length || length>64 ||
       strspn(id,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=length)
        luaL_error(L,"invalid checkpoint operation/id");
    lua_pop(L,1);
}
int LiveCheckpoint(lua_State *L) {
    PredicateGuard(L);
    if(Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    LiveCheckpointArguments(L);
    const char *deny=nullptr;
    bool ok=false,installed=false,admitted=false;
    const auto before=PIC_Ticks;
    {
    // No Lua allocations/errors in the owning native scope. lua_pcall cannot
    // unwind C++ strings or leases when Lua reports allocation failure.
    auto field=[&](const char *name) {
        lua_getfield(L,1,name);
        size_t n=0;const char *v=lua_tolstring(L,-1,&n);
        std::string text(v,n);lua_pop(L,1);return text;
    };
    const auto op=field("op"),id=field("id");
    const auto owner=DOSBOX_MachineServiceOwner();
    const auto found=live_checkpoints.find(id);
    const LiveCheckpointImage *image=found==live_checkpoints.end()?nullptr:&found->second;
    // Startup/root can precede DOS drive/file initialization. Inspect native
    // DOS resources only for the suspended EXEC owner that can be admitted.
    auto mounts=owner.kind==MachineService::Kind::ShellExecGuest ? CheckpointMounts() : std::string();
    uint32_t bp_count=0;
    DebuggerBreakpointInfo bp[256];
    const auto bp_error=Debugger_BreakpointList(bp,256,&bp_count,true);
    if(!Debugger_LiveOwnsStop(owner.token) || strcmp(Debugger_LiveStopReason(),"deliberate") ||
       owner.kind!=MachineService::Kind::ShellExecGuest)
        deny="requires deliberate suspended EXEC stop";
    else if(cpudecoder!=CPU_Core_Normal_Run)
        deny="checkpoint requires normal decoder (translated native code is not captured)";
    else if(!CheckpointInstrumentationAllowed(op=="load",bp_error,bp,bp_count))
        deny="instrumentation retained";
    else if(!GFX_CheckpointReady() || !KEYBOARD_CheckpointNeutral() || !Mouse_CheckpointNeutral())
        deny="input/tasks not neutral";
    else if(mounts.empty())deny="unsupported mount, active directory search, or native file kind";
    else if(op=="load" && (!image || owner.token!=image->owner ||
            dos.psp()!=image->psp || image->program!=RunningProgram || mounts!=image->mounts))
        deny="checkpoint owner/PSP/mount mismatch or missing";
    else if(op=="load" && checkpoint_epoch>=uint64_t(LUA_MAXINTEGER))
        deny="checkpoint context counter exhausted";
    if(!deny) {
    // Allocate metadata/map node before capture. Capture publishes bytes only
    // on success; a refused replacement retains the previous image.
    LiveCheckpointImage prepared;
    if(op=="save") {
        prepared.owner=owner.token;prepared.psp=dos.psp();
        prepared.program=RunningProgram;prepared.mounts=mounts;
    }
    {
    const auto context=DEBUG_NativeExecutionContext();
    if(live_active && !live_attachment.serial)
        live_attachment=DEBUG_AttachNativeExecution(NativeDebug::Frontend::Lua,0);
    const auto attachment=live_active?live_attachment:local_attachment;
    if(DEBUG_AdmitNativeState(attachment,context) &&
       NativeControl::owner().acquire({context.invocation,context.checkpoint},owner,
           DOSBOX_MachineServiceRelation(context.invocation),context.checkpoint,true)==NativeControl::Admission::ok) {
    admitted=true;
    const auto lease=NativeControl::owner().lease();
    struct ReleaseCheckpointLease {
        NativeControl::Lease lease;
        ~ReleaseCheckpointLease(){NativeControl::owner().release(lease);}
    } release{lease};
    if(op=="save") {
        auto &destination=live_checkpoints[id]; // allocate before capture/publication
        ok=SaveState::instance().checkpointSave(prepared.parts,64u*1024u*1024u);
        if(ok) destination=std::move(prepared);
    } else {
        ok=SaveState::instance().checkpointLoad(image->parts);
        installed=ok;
        if(ok && !DEBUG_AdvanceNativeStateContext(attachment,context,lease))
            E_Exit("Restored checkpoint cannot retire prior native context");
        if(ok) {
            // Once original component installation succeeds, failure to rebuild
            // host task bindings or establish the destination stop is fatal.
            // A Lua error/result is catchable and must not leave this guest
            // runnable after an installed checkpoint.
            if(!GFX_CheckpointRebase(LiveNow(),PIC_Ticks))
                E_Exit("Restored checkpoint cannot rebase native tasks");
            if(!Debugger_LiveCheckpointStop(owner.token))
                E_Exit("Restored checkpoint cannot establish native stop");
            ++checkpoint_epoch; checkpoint_owner=owner.token;checkpoint_restored=true;
        }
    }
    }
    } // Release native lease before any allocating Lua result projection.
    if(!ok)deny=!admitted?"checkpoint native ownership unavailable":
        op=="load"?"checkpoint restore refused before installation":"save failed";
    }
    } // Destroy request strings and prepared metadata before projecting Lua.
    lua_newtable(L);
    lua_pushboolean(L,ok);lua_setfield(L,-2,"ok");
    lua_pushboolean(L,installed);lua_setfield(L,-2,"mutation_attempted");
    lua_getfield(L,1,"id");lua_setfield(L,-2,"id");
    lua_pushinteger(L,before);lua_setfield(L,-2,"tick_before");
    lua_pushinteger(L,PIC_Ticks);lua_setfield(L,-2,"tick_after");
    lua_pushinteger(L,checkpoint_epoch);lua_setfield(L,-2,"state_epoch");
    if(deny){lua_pushstring(L,deny);lua_setfield(L,-2,"error");}
    return 1;
}
// Pure projection of the one retained target. Read authority does not confer
// target control authority and must not refresh registers or run maintenance.
static void LiveLastTarget(lua_State *L) {
    const auto &t=Debugger_ExecutionTarget();
    lua_newtable(L);
    auto counter=[&](const char *name,uint64_t value) {char text[32];
        snprintf(text,sizeof(text),"%llu",(unsigned long long)value);
        lua_pushstring(L,text);lua_setfield(L,-2,name);};
    auto number=[&](const char *name,unsigned value) {
        lua_pushinteger(L,value);lua_setfield(L,-2,name);};
    lua_pushstring(L,t.reason);lua_setfield(L,-2,"reason");
    const bool terminal=t.generation && strcmp(t.reason,"armed");
    lua_pushboolean(L,terminal);lua_setfield(L,-2,"terminal");
    counter("generation",t.generation);counter("arm_owner",t.owner);
    number("arm_owner_kind",t.arm_owner_kind);number("arm_owner_depth",t.arm_owner_depth);
    counter("bp_id",t.breakpoint_id);number("cs",t.cs);number("ip",t.ip);
    counter("start_ms",t.start_ms);counter("deadline_ms",t.deadline_ms);
    counter("end_ms",t.end_ms);counter("tick",t.tick);counter("bp_hit_count",t.hit_count);
    counter("terminal_owner",t.terminal_owner);
    number("terminal_owner_kind",t.terminal_owner_kind);number("terminal_owner_depth",t.terminal_owner_depth);
    lua_pushstring(L,!terminal ? "unknown:no-terminal" :
        t.terminal_regs_valid ? "transition-current-owner" : "unknown:decoder-state");
    lua_setfield(L,-2,"regs_state");
    lua_pushstring(L,t.terminal_flags_valid ? "materialized-hit" : "unknown:not-captured");
    lua_setfield(L,-2,"flags_state");
    if(t.terminal_regs_valid) {
        Debugger_LuaPushRegisters(L,t.terminal_regs,t.terminal_flags_valid);
        lua_setfield(L,-2,"regs");
    }
}
int LiveStatus(lua_State *L) {
    lua_newtable(L);Debugger_LuaPredicateSummary(L);auto owner=DOSBOX_MachineServiceOwner();
    LiveLastTarget(L);lua_setfield(L,-2,"last_target");
    lua_pushinteger(L,checkpoint_epoch);lua_setfield(L,-2,"checkpoint_state_epoch");
    lua_pushboolean(L,checkpoint_restored);lua_setfield(L,-2,"checkpoint_dispose_before_return");
    lua_pushboolean(L,Debugger_LiveEffectiveStopped());lua_setfield(L,-2,"stopped");
    lua_pushinteger(L,PIC_Ticks);lua_setfield(L,-2,"guest_tick");
    lua_pushinteger(L,owner.depth);lua_setfield(L,-2,"owner_depth");
    lua_pushstring(L,owner.kind==MachineService::Kind::RootGuest?"RootGuest":"ShellExecGuest");lua_setfield(L,-2,"owner_kind");
    auto counter=[&](const char *name,uint64_t value) {char text[32];snprintf(text,sizeof(text),"%llu",(unsigned long long)value);
        lua_pushstring(L,text);lua_setfield(L,-2,name);};
    counter("owner_token",owner.token);counter("slices",live_slices);
    counter("slice_start_ms",live_start);counter("previous_slice_end_ms",live_end);
    counter("stop_start_ms",Debugger_LiveStopStarted());counter("stop_end_ms",Debugger_LiveStopFinished());
    lua_pushstring(L,Debugger_LiveStopReason());lua_setfield(L,-2,"mode_reason");
    lua_pushstring(L,"live-0.2");lua_setfield(L,-2,"rcp_mode");
    lua_newtable(L);unsigned capability=0;
    for(const char *name:{"status","stop","resume","checkpoint","phase","execution_target","executable_entry","mouse_witness_arm","input","capture","poll","ack","cancel","release","regs_read","reg_read","mem_read","disasm"}) {
        lua_pushstring(L,name);lua_rawseti(L,-2,++capability);
    }
    if(getenv("DOSBOX_Y_EXPERIMENTAL_PREDICATE_GATE_G")) {
        for(const char *name:{"breakpoint_callbacks","script_install"}) {
            lua_pushstring(L,name);lua_rawseti(L,-2,++capability);
        }
    }
    lua_setfield(L,-2,"capabilities");
    lua_newtable(L);
    lua_pushboolean(L,live_transport && live_transport->closed());lua_setfield(L,-2,"closed");
    lua_pushinteger(L,live_transport?unsigned(live_transport->fault()):0);lua_setfield(L,-2,"fault");
    lua_pushboolean(L,live_failed);lua_setfield(L,-2,"dispatch_failed");lua_setfield(L,-2,"channel");
    auto tasks=GFX_LuaTaskStatus();lua_newtable(L);
    auto number=[&](const char *name,unsigned value){lua_pushinteger(L,value);lua_setfield(L,-2,name);};
    number("pending",tasks.pending);number("held",tasks.held);number("frame_receipts",tasks.frames);
    number("lifetime_ids",tasks.lifetime);number("clock_error",tasks.clock_error);number("cleanup_state",tasks.cleanup_state);
    auto taskcounter=[&](const char *name,uint64_t value){char text[32];snprintf(text,sizeof(text),"%llu",(unsigned long long)value);lua_pushstring(L,text);lua_setfield(L,-2,name);};
    taskcounter("guest_ms",tasks.guest_ms);taskcounter("epoch",tasks.epoch);taskcounter("cleanup_generation",tasks.cleanup_generation);
    lua_pushboolean(L,tasks.configured);lua_setfield(L,-2,"configured");
    lua_pushboolean(L,tasks.closed);lua_setfield(L,-2,"closed");
    lua_pushboolean(L,tasks.frame_exhausted);lua_setfield(L,-2,"frame_exhausted");lua_setfield(L,-2,"tasks");
    auto geometry=GFX_LuaGeometry();counter("profile_generation",geometry.generation);
    lua_newtable(L);
    auto scalar=[&](const char *name,double value) {lua_pushnumber(L,value);lua_setfield(L,-2,name);};
    scalar("requested_x",geometry.requested_x);scalar("requested_y",geometry.requested_y);
    scalar("host_x",geometry.host.x);scalar("host_y",geometry.host.y);
    scalar("relative_x",geometry.host.dx);scalar("relative_y",geometry.host.dy);
    scalar("dispatch_tick",geometry.dispatch_tick);
    scalar("clip_x",geometry.profile.viewport.x);scalar("clip_y",geometry.profile.viewport.y);
    scalar("clip_w",geometry.profile.viewport.width);scalar("clip_h",geometry.profile.viewport.height);
    scalar("screen_x",geometry.mouse.screen_x);scalar("screen_y",geometry.mouse.screen_y);
    scalar("min_x",geometry.mouse.min_x);scalar("max_x",geometry.mouse.max_x);
    scalar("min_y",geometry.mouse.min_y);scalar("max_y",geometry.mouse.max_y);
    scalar("absolute_x",geometry.mouse.absolute_x);scalar("absolute_y",geometry.mouse.absolute_y);
    scalar("motion_tick",geometry.mouse.motion_tick);
    scalar("user_width",geometry.profile.user_width);scalar("user_height",geometry.profile.user_height);
    scalar("user_emulation",geometry.profile.user_emulation);scalar("notify",geometry.profile.notify);
    scalar("internal_x",geometry.mouse.internal_x);scalar("internal_y",geometry.mouse.internal_y);
    // Read-only typed bounded witness; no Lua invocation from mouse callbacks.
    auto event=[&](const MouseWitnessEvent &e) {
        lua_newtable(L);
        auto n=[&](const char *key,double value){lua_pushnumber(L,value);lua_setfield(L,-2,key);};
        char seq[32];snprintf(seq,sizeof(seq),"%llu",(unsigned long long)e.sequence);
        lua_pushstring(L,seq);lua_setfield(L,-2,"sequence");
        if(e.route==MouseWitnessRoute::ModeBefore || e.route==MouseWitnessRoute::ModeAfter) {
            n("mode",e.mode);n("operation",e.operation);n("callback_cs",e.callback_cs);n("callback_ip",e.callback_ip);
            n("return_cs",e.return_cs);n("return_ip",e.return_ip);
            lua_pushboolean(L,e.return_valid);lua_setfield(L,-2,"return_valid");
        }
        n("tick",e.tick);n("route",unsigned(e.route));n("x",e.x);n("y",e.y);
        n("screen_x",e.screen_x);n("screen_y",e.screen_y);n("min_x",e.min_x);n("max_x",e.max_x);n("min_y",e.min_y);n("max_y",e.max_y);
        n("gran_x",e.gran_x);n("gran_y",e.gran_y);if(e.route!=MouseWitnessRoute::ModeBefore && e.route!=MouseWitnessRoute::ModeAfter) {
            n("ax",e.ax);n("bx",e.bx);n("cx",e.cx);n("dx",e.dx);n("dest_seg",e.dest_seg);n("dest_off",e.dest_off);
        }
    };
    lua_newtable(L);
    const auto &first=geometry.mouse.first;
    lua_pushstring(L,first.action_id);lua_setfield(L,-2,"action_id");
    char gen[32];snprintf(gen,sizeof(gen),"%llu",(unsigned long long)first.generation);
    lua_pushstring(L,gen);lua_setfield(L,-2,"generation");
    auto flag=[&](const char *key,bool b){lua_pushboolean(L,b);lua_setfield(L,-2,key);};
    flag("armed",first.armed);flag("matched",first.matched);flag("frozen",first.frozen);flag("overflow",first.overflow);flag("exhausted",first.exhausted);
    lua_newtable(L);for(unsigned i=0;i<first.count;i++){event(first.events[i]);lua_rawseti(L,-2,i+1);}
    lua_setfield(L,-2,"events");lua_setfield(L,-2,"first_after_motion");
    event(geometry.mouse.motion_end);lua_setfield(L,-2,"motion_end");
    lua_newtable(L);
    for(unsigned i=0;i<geometry.mouse.recent.count;i++){event(geometry.mouse.recent.events[i]);lua_rawseti(L,-2,i+1);}
    lua_setfield(L,-2,"recent_events");
    char lost[32];snprintf(lost,sizeof(lost),"%llu",(unsigned long long)geometry.mouse.recent.overwritten);
    lua_pushstring(L,lost);lua_setfield(L,-2,"witness_overwritten");
    lua_pushboolean(L,geometry.mouse.recent.exhausted);lua_setfield(L,-2,"witness_exhausted");
    scalar("returned_x",geometry.mouse.returned_x);scalar("returned_y",geometry.mouse.returned_y);
    scalar("return_tick",geometry.mouse.return_tick);counter("return_sequence",geometry.mouse.return_sequence);
    scalar("x_sensitivity",geometry.profile.xsensitivity);scalar("y_sensitivity",geometry.profile.ysensitivity);
    scalar("emulation",geometry.profile.emulation);scalar("lock",geometry.profile.lock);
    lua_pushboolean(L,geometry.motion);lua_setfield(L,-2,"has_motion");
    lua_setfield(L,-2,"geometry");return 1;
}
int LiveMouseWitnessArm(lua_State *L) {
    if(lua_gettop(L)!=1 || lua_type(L,1)!=LUA_TSTRING)return luaL_error(L,"invalid action_id");
    size_t length=0;const char *id=lua_tolstring(L,1,&length);
    if(length!=strlen(id) || !NativeInput::task_identifier(id))return luaL_error(L,"invalid action_id");
    if(!Mouse_ArmFirstWitness(id))return luaL_error(L,"witness generation exhausted");
    lua_newtable(L);lua_pushboolean(L,true);lua_setfield(L,-2,"armed");
    lua_pushstring(L,id);lua_setfield(L,-2,"action_id");return 1;
}
int LiveStop(lua_State *L) {
    PredicateGuard(L);
    if(!Debugger_LiveStop(DOSBOX_MachineServiceOwner().token))return luaL_error(L,"unsupported-context: stop owner");
    return LiveStatus(L);
}
// One fresh-launch admission, one LOADNGO receipt. No Lua in DOS callbacks.
struct EntryHold {
 bool launch=false,armed=false,hit=false,expired=false;
 uint64_t owner=0,generation=0,deadline=0;
 uint16_t psp=0,load=0,cs=0,ip=0;
 uint64_t hold_ms=0;bool lean=false,load_eligible=false;
 std::string program,executable; // actual opened host-relative path, or empty refusal
} entry_hold;
// Provenance is an observation of natural LOADNGO, not an entry-stop policy.
// Owner token and PSP bind it to that guest frame; later loads replace it.
struct NaturalExecutableReceipt {
 uint64_t owner=0;
 uint16_t psp=0;
 std::string executable;
} natural_executable;
static void RecordNaturalExecutable(const char *executable,uint16_t psp) noexcept {
 const auto owner=DOSBOX_MachineServiceOwner();
 // Invalidate first, including failed allocation or an unqualified load.
 natural_executable.owner=0;natural_executable.psp=0;natural_executable.executable.clear();
 if(owner.kind!=MachineService::Kind::ShellExecGuest || owner.depth!=2 ||
    !owner.token || !psp || !executable || !*executable)return;
 // LOADNGO has already changed guest registers/stack. Provenance is optional
 // metadata: allocation failure must deny save, not unwind guest execution.
 try {natural_executable.executable=executable;}
 catch(...) {natural_executable.executable.clear();return;}
 natural_executable.psp=psp;natural_executable.owner=owner.token;
}

int LiveResume(lua_State *L) {
    PredicateGuard(L);
    Debugger_EntryMaintain();
    if(entry_hold.launch && !entry_hold.generation)return luaL_error(L,"entry launch requires arm");
    if(entry_hold.launch && entry_hold.expired)return luaL_error(L,"entry hold expired; dispose session");
    if(Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    const auto tasks=GFX_LuaTaskStatus();
    const auto &target=Debugger_ExecutionTarget();
    if(!strcmp(target.reason,"hit") && Debugger_LiveOwnsStop(target.owner) &&
       (tasks.pending || tasks.held || tasks.cleanup_state==unsigned(NativeInput::CleanupState::Failed)))
        return luaL_error(L,"target resume requires settled input cleanup");
    if(!Debugger_LiveResume(DOSBOX_MachineServiceOwner().token))return luaL_error(L,"unsupported-context: resume owner/genuine stop");
    if(entry_hold.hit && entry_hold.owner==DOSBOX_MachineServiceOwner().token)entry_hold.launch=false;
    return LiveStatus(L);
}
void LiveFields(lua_State *L,int table,const char *const *allowed) {
    table=lua_absindex(L,table);lua_pushnil(L);
    while(lua_next(L,table)) {
        if(lua_type(L,-2)!=LUA_TSTRING)luaL_error(L,"invalid field name");
        size_t key_length=0;const char *name=lua_tolstring(L,-2,&key_length);
        if(memchr(name,0,key_length))luaL_error(L,"invalid field name");
        bool found=false;
        for(unsigned i=0;allowed[i];i++)if(!strcmp(name,allowed[i]))found=true;
        if(!found)luaL_error(L,"unknown field: %s",name);
        if(lua_type(L,-1)==LUA_TFUNCTION)luaL_error(L,"callbacks forbidden");
        lua_pop(L,1);
    }
}
const char *LiveString(lua_State *L,int index) {
    luaL_checktype(L,index,LUA_TSTRING);
    size_t size=0;
    const char *text=lua_tolstring(L,index,&size);
    if(memchr(text,0,size))luaL_error(L,"embedded NUL in string");
    return text;
}
lua_Integer LiveInteger(lua_State *L,int index) {
    if(!lua_isinteger(L,index))luaL_error(L,"integer required");
    return lua_tointeger(L,index);
}
// Fail-closed native admission boundary. This is intentionally not advertised
// as state_file.v1 until manifest, lease and fresh-entry jobs are implemented.
// Never delegate persistence to the synchronous legacy checkpoint route.
int LiveStateFile(lua_State *L) {
    PredicateGuard(L);
    if(Debugger_PredicateHeld())return luaL_error(L,"state-file admission unavailable during predicate settlement");
    if(lua_gettop(L)!=1 || !lua_istable(L,1))return luaL_error(L,"state_file requires object");
    if(lua_getmetatable(L,1))return luaL_error(L,"state_file requires plain object");
    lua_getfield(L,1,"op");
    if(lua_type(L,-1)!=LUA_TSTRING)return luaL_error(L,"state_file requires operation");
    size_t op_length=0;
    const char *op=lua_tolstring(L,-1,&op_length);
    const bool readiness=op_length==9 && !memcmp(op,"readiness",9);
    const bool save=op_length==4 && !memcmp(op,"save",4);
    const bool load=op_length==4 && !memcmp(op,"load",4);
    const bool inspect=op_length==7 && !memcmp(op,"inspect",7);
    lua_pop(L,1);
    const char *read_fields[]={"op","expected","profile",nullptr};
    const char *job_fields[]={"op","expected","operation_id","id","wall_ms",nullptr};
    const char *load_fields[]={"op","expected","operation_id","id","wall_ms","manifest_sha256",nullptr};
    LiveFields(L,1,readiness ? read_fields : load ? load_fields : job_fields);
    if(!readiness && !save && !load && !inspect)return luaL_error(L,"invalid state_file operation");
    lua_getfield(L,1,"expected");
    if(!lua_istable(L,-1))return luaL_error(L,"state_file expected identity required");
    if(lua_getmetatable(L,-1))return luaL_error(L,"state_file requires plain identity");
    const char *identity_fields[]={"instance_id","owner_token","state_epoch",nullptr};
    LiveFields(L,-1,identity_fields);
    for(const char *const *field=identity_fields;*field;++field) {
        lua_getfield(L,-1,*field);
        if(lua_type(L,-1)!=LUA_TSTRING)return luaL_error(L,"state_file identity must use canonical strings");
        size_t length=0;const char *text=lua_tolstring(L,-1,&length);
        if(!strcmp(*field,"instance_id")) {
            if(length!=32 || strspn(text,"0123456789abcdef")!=length)
                return luaL_error(L,"invalid state_file instance identity");
        } else if(!length || length>20 || strspn(text,"0123456789")!=length ||
                  (length>1 && text[0]=='0') ||
                  (length==20 && strcmp(text,"18446744073709551615")>0))
            return luaL_error(L,"invalid state_file counter");
        lua_pop(L,1);
    }
    lua_pop(L,1);
    // There is no admitted instance/profile/root descriptor yet. Do not return
    // ready=false under an invented identity, allocate a job, or touch files.
    return luaL_error(L,"unsupported: native persistent manifest/lease admission not installed");
}

#include "lean_live.inc"

int LiveTask(lua_State *L) {
    luaL_checktype(L,1,LUA_TTABLE);
    NativeInput::Request request;
    auto text=[&](const char *field,char *out) {
        lua_getfield(L,1,field);const char *value=LiveString(L,-1);
        if(!NativeInput::task_identifier(value) && !(request.op==NativeInput::Operation::Poll && !strcmp(value,"@cleanup")))luaL_error(L,"invalid task identifier");
        strcpy(out,value);lua_pop(L,1);
    };
    lua_getfield(L,1,"op");const char *op=LiveString(L,-1);
    if(!strcmp(op,"input"))request.op=NativeInput::Operation::Input;
    else if(!strcmp(op,"capture"))request.op=NativeInput::Operation::Capture;
    else if(!strcmp(op,"poll"))request.op=NativeInput::Operation::Poll;
    else if(!strcmp(op,"ack"))request.op=NativeInput::Operation::Ack;
    else if(!strcmp(op,"cancel"))request.op=NativeInput::Operation::Cancel;
    else if(!strcmp(op,"release"))request.op=NativeInput::Operation::Release;
    else return luaL_error(L,"invalid task operation");
    if(request.op!=NativeInput::Operation::Poll) {
        PredicateGuard(L);
        if(Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    }
    lua_pop(L,1);
    static const char *input_fields[]={"op","action_id","deadline_ms","events",nullptr};
    static const char *capture_fields[]={"op","action_id","deadline_ms",nullptr};
    static const char *control_fields[]={"op","action_id",nullptr};
    static const char *release_fields[]={"op",nullptr};
    LiveFields(L,1,request.op==NativeInput::Operation::Input?input_fields:
        request.op==NativeInput::Operation::Capture?capture_fields:
        request.op==NativeInput::Operation::Release?release_fields:control_fields);
    if(request.op==NativeInput::Operation::Release)strcpy(request.id,"release");
    else text("action_id",request.id);
    if(request.op==NativeInput::Operation::Input || request.op==NativeInput::Operation::Capture) {
        lua_getfield(L,1,"deadline_ms");lua_Integer deadline=LiveInteger(L,-1);lua_pop(L,1);
        if(deadline<1 || deadline>2000)return luaL_error(L,"invalid deadline");
        request.deadline_ms=uint32_t(deadline);
    } else strcpy(request.target,request.id);
    if(request.op==NativeInput::Operation::Input) {
        lua_getfield(L,1,"events");luaL_checktype(L,-1,LUA_TTABLE);size_t count=lua_rawlen(L,-1);
        if(!count || count>16)return luaL_error(L,"invalid event count");
        request.count=unsigned(count);
        const int events_table=lua_absindex(L,-1);size_t members=0;lua_pushnil(L);
        while(lua_next(L,events_table)) {
            if(!lua_isinteger(L,-2) || lua_tointeger(L,-2)<1 || size_t(lua_tointeger(L,-2))>count)return luaL_error(L,"non-array events");
            ++members;lua_pop(L,1);
        }
        if(members!=count)return luaL_error(L,"sparse events");
        for(unsigned i=0;i<count;i++) {
            lua_rawgeti(L,-1,i+1);luaL_checktype(L,-1,LUA_TTABLE);auto &event=request.events[i];
            static const char *event_fields[]={"key","button","x","y","down","after_ms",nullptr};
            LiveFields(L,-1,event_fields);
            unsigned kinds=0;
            for(const char *field:{"key","button","x"}) {lua_getfield(L,-1,field);kinds+=!lua_isnil(L,-1);lua_pop(L,1);}
            if(kinds!=1)return luaL_error(L,"ambiguous event");
            lua_getfield(L,-1,"after_ms");lua_Integer delay=LiveInteger(L,-1);lua_pop(L,1);
            if(delay<0 || delay>1000 || (i && delay<request.events[i-1].after_ms))return luaL_error(L,"invalid event offset");
            event.after_ms=uint32_t(delay);
            lua_getfield(L,-1,"key");
            if(!lua_isnil(L,-1)) {const char *name=LiveString(L,-1);if(!NativeInput::key_index(name,event.index))return luaL_error(L,"unsupported key");lua_pop(L,1);}
            else {
                lua_pop(L,1);lua_getfield(L,-1,"button");
                if(!lua_isnil(L,-1)) {lua_Integer button=LiveInteger(L,-1);if(button<0 || button>2)return luaL_error(L,"unsupported button");event.index=NativeInput::KeyCount+unsigned(button);lua_pop(L,1);}
                else {
                    lua_pop(L,1);event.kind=NativeInput::Event::Kind::Viewport;
                    lua_getfield(L,-1,"x");lua_Integer x=LiveInteger(L,-1);lua_pop(L,1);
                    lua_getfield(L,-1,"y");lua_Integer y=LiveInteger(L,-1);lua_pop(L,1);
                    if(x<0 || x>65535 || y<0 || y>65535)return luaL_error(L,"invalid viewport");
                    event.x=uint32_t(x);event.y=uint32_t(y);
                }
            }
            if(event.kind==NativeInput::Event::Kind::Viewport) {
                lua_getfield(L,-1,"down");if(!lua_isnil(L,-1))return luaL_error(L,"motion down forbidden");lua_pop(L,1);
            } else {
                lua_getfield(L,-1,"y");if(!lua_isnil(L,-1))return luaL_error(L,"edge coordinates forbidden");lua_pop(L,1);
            }
            if(event.kind!=NativeInput::Event::Kind::Viewport) {lua_getfield(L,-1,"down");luaL_checktype(L,-1,LUA_TBOOLEAN);event.down=lua_toboolean(L,-1);lua_pop(L,1);}
            lua_pop(L,1);
        }
        lua_pop(L,1);
    }
    if(entry_hold.launch && Debugger_LiveEffectiveStopped() &&
       request.op!=NativeInput::Operation::Poll && request.op!=NativeInput::Operation::Ack)
        return luaL_error(L,"entry hold forbids input/capture tasks");
    std::string result=GFX_ExecuteLuaTask(request,!Debugger_LiveEffectiveStopped());
    if(result.empty())return luaL_error(L,"task owner unavailable");
    lua_pushlstring(L,result.data(),result.size());return 1;
}
// Data-only target lifecycle; normal machine loop owns execution.
int LiveExecutableEntry(lua_State *L) {
 PredicateGuard(L);
 if(lua_gettop(L)!=1 || !lua_istable(L,1))return luaL_error(L,"executable_entry requires object");
 lua_getfield(L,1,"op");luaL_checktype(L,-1,LUA_TSTRING);std::string op=lua_tostring(L,-1);lua_pop(L,1);
 static const char *arm[]={"op","program","wall_ms",nullptr};
 static const char *query[]={"op","generation",nullptr};
 LiveFields(L,1,op=="arm"?arm:query);
 const auto owner=DOSBOX_MachineServiceOwner();
 Debugger_EntryMaintain();
 if(op=="arm") {
  if(!entry_hold.launch || entry_hold.generation || entry_hold.expired ||
     owner.token!=entry_hold.owner || !Debugger_LiveOwnsStop(owner.token))
   return luaL_error(L,"requires fresh owned launch hold");
  auto tasks=GFX_LuaTaskStatus();
  if(tasks.pending || tasks.held)return luaL_error(L,"requires settled input");
  lua_getfield(L,1,"program");luaL_checktype(L,-1,LUA_TSTRING);
  size_t n=0;const char *program=lua_tolstring(L,-1,&n);
  if(!n || n>255 || strlen(program)!=n)return luaL_error(L,"invalid program");
  const std::string selected(program,n);lua_pop(L,1);
  lua_getfield(L,1,"wall_ms");auto wall=LiveInteger(L,-1);lua_pop(L,1);
  if(wall<1 || wall>10000)return luaL_error(L,"invalid duration");
  entry_hold.program=selected;entry_hold.deadline=LiveNow()+wall;entry_hold.generation=1;entry_hold.armed=true;
 } else if(op=="poll" || op=="cancel") {
  lua_getfield(L,1,"generation");auto gen=LiveInteger(L,-1);lua_pop(L,1);
  if(gen!=1 || !entry_hold.generation || owner.token!=entry_hold.owner)
   return luaL_error(L,"stale-generation-or-owner");
  if(op=="cancel") {
   if(entry_hold.hit)return luaL_error(L,"loaded entry cannot cancel; retain hold or dispose session");
   entry_hold.armed=false;entry_hold.expired=true;
  }
 } else return luaL_error(L,"invalid entry operation");
 lua_newtable(L);
 lua_pushstring(L,entry_hold.expired?"nonarrival":entry_hold.hit?"loaded":"armed");lua_setfield(L,-2,"reason");
 auto num=[&](const char *key,uint64_t v){lua_pushinteger(L,v);lua_setfield(L,-2,key);};
 num("generation",entry_hold.generation);num("owner",entry_hold.owner);num("psp",entry_hold.psp);
 num("load_segment",entry_hold.load);num("cs",entry_hold.cs);num("ip",entry_hold.ip);
 num("deadline_ms",entry_hold.deadline);
 lua_pushstring(L,entry_hold.program.c_str());lua_setfield(L,-2,"program");
 return 1;
}
int LiveExecutionTarget(lua_State *L) {
    PredicateGuard(L);
    if(Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
 if (lua_gettop(L)!=1 || !lua_istable(L,1)) return luaL_error(L,"execution_target requires object");
 lua_getfield(L,1,"op");luaL_checktype(L,-1,LUA_TSTRING);const std::string op=lua_tostring(L,-1);lua_pop(L,1);
 static const char *arm_fields[]={"op","cs","ip","wall_ms",nullptr};
 static const char *query_fields[]={"op","generation",nullptr};
 LiveFields(L,1,op=="arm" ? arm_fields : query_fields);
 const char *failure=nullptr;
 if (op=="arm") {
  const auto tasks=GFX_LuaTaskStatus();
  if(tasks.pending || tasks.held || tasks.cleanup_state==unsigned(NativeInput::CleanupState::Failed))
   return luaL_error(L,"target requires settled input cleanup (no pending or held input)");
  auto integer=[&](const char *key,lua_Integer max) {
   lua_getfield(L,1,key);auto v=LiveInteger(L,-1);lua_pop(L,1);
   if(v<0 || v>max)luaL_error(L,"invalid %s",key);return v;
  };
  auto cs=integer("cs",65535),ip=integer("ip",65535),wall=integer("wall_ms",10000);
  failure=Debugger_ExecutionArm(uint16_t(cs),uint16_t(ip),uint32_t(wall));
 } else if(op=="poll" || op=="cancel") {
  lua_getfield(L,1,"generation");auto generation=LiveInteger(L,-1);lua_pop(L,1);
  const auto &t=Debugger_ExecutionTarget();
  if(generation<=0 || uint64_t(generation)!=t.generation || t.owner!=DOSBOX_MachineServiceOwner().token)
   return luaL_error(L,"stale-generation-or-owner");
  if(op=="cancel") failure=Debugger_ExecutionCancel(uint64_t(generation));
 } else return luaL_error(L,"invalid target operation");
 if(failure)return luaL_error(L,"%s",failure);
 const auto &t=Debugger_ExecutionTarget();lua_newtable(L);
 lua_pushstring(L,t.reason);lua_setfield(L,-2,"reason");
 auto number=[&](const char *key,uint64_t v){lua_pushinteger(L,static_cast<lua_Integer>(v));lua_setfield(L,-2,key);};
 number("generation",t.generation);number("owner",t.owner);number("bp_id",t.breakpoint_id);
 number("cs",t.cs);number("ip",t.ip);number("start_ms",t.start_ms);number("end_ms",t.end_ms);
 number("deadline_ms",t.deadline_ms);number("tick",t.tick);number("bp_hit_count",t.hit_count);
 if(!strcmp(t.reason,"hit")){Debugger_LuaPushRegisters(L,t.regs);lua_setfield(L,-2,"regs");}
 return 1;
}
static uint64_t live_phase_operation=0;
static lua_State *live_dispatch_thread=nullptr;
static int live_dispatch_ref=LUA_NOREF;
static uint64_t live_phase_start=0;
static uint32_t live_phase_tick=0;
int LivePhaseResult(lua_State *L, int, lua_KContext) {
    const auto *result=DEBUG_NativePhaseResult(live_attachment,live_phase_operation);
    if (!result) {
        if (const auto *terminal=DEBUG_NativeExecutionResult(live_attachment)) {
            if (terminal->operation==live_phase_operation)
                DEBUG_ReleaseNativeExecutionResult(live_attachment,live_phase_operation);
        }
        live_phase_operation=0;
        return luaL_error(L,"phase result unavailable; dispose session");
    }
    const auto phase=*result;
    DEBUG_ReleaseNativeExecutionResult(live_attachment,live_phase_operation);
    live_phase_operation=0;
    Debugger_LivePhaseCompleted(phase.event);
    lua_newtable(L);
    lua_pushstring(L,phase.reason?phase.reason:"event");lua_setfield(L,-2,"reason");
    Debugger_LuaSetIntegerField(L,"slices",phase.slices);
    Debugger_LuaSetIntegerField(L,"start_ms",live_phase_start);
    Debugger_LuaSetIntegerField(L,"end_ms",LiveNow());
    Debugger_LuaSetIntegerField(L,"tick_before",live_phase_tick);
    Debugger_LuaSetIntegerField(L,"tick_after",PIC_Ticks);
    Debugger_LuaPushStopEvent(L,phase.event);lua_setfield(L,-2,"event");
    return 1;
}
// Live transport suspends the exact dispatcher, never replays its request.
int LivePhase(lua_State *L) {
    PredicateGuard(L);
    if (local_evaluation || local_closing)
        return luaL_error(L,"managed evaluation requires deferred execution; synchronous phase unavailable");
    if(Debugger_PredicateHeld())return luaL_error(L,"control unavailable during provisional predicate settlement");
    if(PredicateEnabled())return luaL_error(L,"synchronous guest execution requires predicate detachment");
    if (lua_gettop(L)!=1 || !lua_istable(L,1)) return luaL_error(L,"phase requires an object");
    static const char *fields[]={"instruction_limit","wall_ms","cs","ip",nullptr};
    LiveFields(L,1,fields);
    auto integer=[&](const char *name,lua_Integer max) {
        lua_getfield(L,1,name);lua_Integer v=LiveInteger(L,-1);lua_pop(L,1);
        if(v<0 || v>max) luaL_error(L,"invalid %s",name);
        return v;
    };
    const auto limit=integer("instruction_limit",10000);
    const auto wall=integer("wall_ms",100);
    if (!limit || !wall) return luaL_error(L,"phase budgets must be positive");
    lua_getfield(L,1,"cs");bool target=!lua_isnil(L,-1);lua_pop(L,1);
    lua_getfield(L,1,"ip");bool have_ip=!lua_isnil(L,-1);lua_pop(L,1);
    if(target!=have_ip) return luaL_error(L,"phase requires both cs and ip");
    uint16_t cs=0,ip=0;
    if(target) {cs=uint16_t(integer("cs",65535));ip=uint16_t(integer("ip",65535));}
    if (!live_active || !Debugger_LiveOwnsStop(DOSBOX_MachineServiceOwner().token))
        return luaL_error(L,"unsupported-context: phase requires same-owner deliberate stop");
    const auto tasks=GFX_LuaTaskStatus();
    if(tasks.pending || tasks.held) return luaL_error(L,"phase requires no pending input/capture or held keys");
    if (L!=live_dispatch_thread || !lua_isyieldable(L))
        return luaL_error(L,"phase requires yieldable live dispatcher");
    if (!live_attachment.serial)
        live_attachment=DEBUG_AttachNativeExecution(NativeDebug::Frontend::Lua,0);
    NativeDebug::Request request;
    request.frontend=NativeDebug::Frontend::Lua;request.attachment=live_attachment;
    request.expected=DEBUG_NativeExecutionContext();request.retain_result=true;
    request.action=NativeDebug::Request::Action::Phase;
    request.phase_limit=uint32_t(limit);request.phase_wall_ms=uint32_t(wall);
    request.phase_target=target;request.phase_cs=cs;request.phase_ip=ip;
    if (DEBUG_RequestNativeExecution(request)!=NativeDebug::Admission::Accepted)
        return luaL_error(L,"phase native admission refused");
    live_phase_operation=DEBUG_NativeExecutionOperation();
    live_phase_start=LiveNow();live_phase_tick=PIC_Ticks;
    return lua_yieldk(L,0,0,LivePhaseResult);
}
void LiveHook(lua_State *L,lua_Debug *) {
    if(predicate_active)predicate_budget_failed=true;
    luaL_error(L,"live instruction budget exhausted");
}
// Initialization and module hooks may allocate or raise through C modules.
// Keep them beneath the same Lua error boundary as ordinary dispatch.
int LiveInitializeLean(lua_State *L) {
    luaL_openlibs(L);LeanRegister(L);
    if(luaL_loadfile(L,live_module)!=LUA_OK)return lua_error(L);
    lua_call(L,0,1);
    if(!lua_istable(L,-1))return luaL_error(L,"lean module must return dispatcher table");
    lua_getfield(L,-1,"live_dispatch");const bool dispatch=lua_isfunction(L,-1);lua_pop(L,1);
    lua_getfield(L,-1,"live_finish");const bool finish=lua_isfunction(L,-1);lua_pop(L,1);
    if(!dispatch || !finish)return luaL_error(L,"invalid lean dispatcher");
    lua_setglobal(L,"live_dispatcher");
    return 0;
}
int LiveInitialize(lua_State *L) {
    luaL_openlibs(L);PredicateInstallLoadGuards(L);Debugger_LuaRegisterDbg(L);
    lua_getglobal(L,"dbg");lua_newtable(L);
    lua_pushcfunction(L,LiveExecutableEntry);lua_setfield(L,-2,"executable_entry");
    lua_pushcfunction(L,LiveExecutionTarget);lua_setfield(L,-2,"execution_target");
    lua_pushcfunction(L,LiveCheckpoint);lua_setfield(L,-2,"checkpoint");
    lua_pushcfunction(L,LiveStateFile);lua_setfield(L,-2,"state_file");
    lua_pushcfunction(L,LivePhase);lua_setfield(L,-2,"phase");
    lua_pushcfunction(L,LiveTask);lua_setfield(L,-2,"task");
    lua_pushcfunction(L,LiveStatus);lua_setfield(L,-2,"status");
    lua_pushcfunction(L,LiveMouseWitnessArm);lua_setfield(L,-2,"mouse_witness_arm");
    lua_pushcfunction(L,LiveStop);lua_setfield(L,-2,"stop");
    lua_pushcfunction(L,LiveResume);lua_setfield(L,-2,"resume");
    lua_setfield(L,-2,"live");lua_pop(L,1);
    if(NativeControl::leanMode())LeanRegister(L);
    if(luaL_loadfile(L,live_module)!=LUA_OK)return lua_error(L);
    lua_call(L,0,1);
    if(!lua_istable(L,-1))return luaL_error(L,"live module must return dispatcher table");
    lua_setglobal(L,"live_dispatcher");
    const char *hooks=getenv("DOSBOX_Y_LUA_HOOK_FILE");
    if(hooks && *hooks) {
        if(luaL_loadfile(L,hooks)!=LUA_OK)return lua_error(L);
        lua_call(L,0,0);
    }
    return 0;
}
// Protect argument/metadata/closing projection too, not only module calls.
// A post-effect encoding failure means lost/unknown transport, never rollback.
int LiveDispatchFinish(lua_State *live_lua, int, lua_KContext) {
    lua_State *L=live_lua;
        {
            const uint64_t dispatch_end=LiveNow();
            lua_getglobal(live_lua,"live_dispatcher");lua_getfield(live_lua,-1,"live_finish");lua_remove(live_lua,-2);
            lua_insert(live_lua,-2); // finish function, dispatched response
            lua_newtable(live_lua);
            auto counter=[&](const char *name,uint64_t value) {char text[32];snprintf(text,sizeof(text),"%llu",(unsigned long long)value);
                lua_pushstring(live_lua,text);lua_setfield(live_lua,-2,name);};
            counter("sequence",live_slices);counter("host_start_ms",live_start);counter("dispatch_end_ms",dispatch_end);
            counter("owner_token",DOSBOX_MachineServiceOwner().token);
            lua_pushinteger(live_lua,PIC_Ticks);lua_setfield(live_lua,-2,"guest_tick");
            lua_call(live_lua,2,1);
        }
    if(NativeControl::leanMode()) {
        lua_getglobal(L,"live_dispatcher");lua_getfield(L,-1,"closing");
        lean_protocol_closing=lua_toboolean(L,-1);lua_pop(L,2);
    }
    return 1;
}
int LiveDispatchProtected(lua_State *L) {
    lua_getglobal(L,"live_dispatcher");lua_getfield(L,-1,"live_dispatch");lua_remove(L,-2);
    lua_pushlstring(L,live_line.data(),live_line.size());
    lua_callk(L,1,1,0,LiveDispatchFinish);
    return LiveDispatchFinish(L,LUA_OK,0);
}
int LivePrepareDispatch(lua_State *L) {
    // Publish the registry reference only after Lua has retained the thread.
    // Allocation failure before that point leaves no dangling C++ thread owner.
    lua_State *thread=lua_newthread(L);
    const int reference=luaL_ref(L,LUA_REGISTRYINDEX);
    live_dispatch_thread=thread;live_dispatch_ref=reference;
    lua_pushcfunction(thread,LiveDispatchProtected);
    return 0;
}
int LivePredicateInvoke(lua_State *L) {
    lua_rawgeti(L,LUA_REGISTRYINDEX,predicate_ref);
    lua_newtable(L);
    char sequence[32];snprintf(sequence,sizeof(sequence),"%llu",(unsigned long long)Debugger_PredicateSummary().sequence);
    lua_pushstring(L,sequence);lua_setfield(L,-2,"sequence");
    lua_pushliteral(L,"execute");lua_setfield(L,-2,"kind");
    lua_pushliteral(L,"lua");lua_setfield(L,-2,"owner");
    lua_pushinteger(L,predicate_id);lua_setfield(L,-2,"id");
    lua_pushinteger(L,predicate_hits);lua_setfield(L,-2,"hit_count");
    lua_pushinteger(L,Debugger_PredicateTick());lua_setfield(L,-2,"guest_tick");
    lua_pushboolean(L,Debugger_PredicateTasksUnchanged());lua_setfield(L,-2,"tasks_unchanged");
    lua_pushinteger(L,predicate_cs);lua_setfield(L,-2,"cs");
    lua_pushinteger(L,predicate_ip);lua_setfield(L,-2,"ip");
    Debugger_LuaPushRegisters(L,Debugger_PredicateRegisters());lua_setfield(L,-2,"regs");
    lua_call(L,1,1);
    return 1;
}
void LiveSliceBody() {
    if(close_pending && !Debugger_PredicatePending()) {
        assert(!predicate_active && !live_active);
        live_dispatch_thread=nullptr;live_dispatch_ref=LUA_NOREF;live_phase_operation=0;
        PredicateReleaseRegistrations();
        if(live_lua) {
            predicate_closing=true;lua_close(live_lua);predicate_closing=false;
            live_lua=nullptr;predicate_state=nullptr;
        }
        LeanSettle("channel_lost");
        // Admission is closed and Lua/predicate close frames have unwound.
        // The detach boundary retires successful slots, but retains this
        // attachment when host cleanup failed or installation poisoned the job.
        close_pending=false;
        if (DEBUG_DetachNativeExecution(live_attachment)) {live_attachment={};local_attachment={};}
        fprintf(stderr,"Lua debugger: persistent state closed after channel failure\n");
        return;
    }
    if(!local_load_path.empty()) {
        if (!NativeControl::rawEffectAllowed()) return;
        live_lua=luaL_newstate();if(!live_lua){live_failed=true;local_load_path.clear();return;}
        persistent_local=true;predicate_state=live_lua;
        lua_sethook(live_lua,LiveHook,LUA_MASKCOUNT,100000);
        lua_pushcfunction(live_lua,LocalEvaluateFile);
        lua_pushlightuserdata(live_lua,const_cast<char *>(local_load_path.c_str()));
        const bool ok=lua_pcall(live_lua,1,0,0)==LUA_OK;
        if(!ok) {
            fprintf(stderr,"Lua debugger: persistent local load failed: %s\n",
                lua_type(live_lua,-1)==LUA_TSTRING ? lua_tostring(live_lua,-1) : "non-string error");
            PredicateReleaseRegistrations();
            predicate_closing=true;lua_close(live_lua);predicate_closing=false;
            live_lua=nullptr;predicate_state=nullptr;persistent_local=false;
        } else {lua_settop(live_lua,0);fprintf(stderr,"Lua debugger: persistent local ready\n");}
        local_load_path.clear();if(!ok)live_failed=true;return;
    }
    if(Debugger_PredicatePending()) {
        assert(live_lua && !live_active && !predicate_active);
        const int top=lua_gettop(live_lua);
        const auto hook=lua_gethook(live_lua);const int mask=lua_gethookmask(live_lua),count=lua_gethookcount(live_lua);
        predicate_active=true;predicate_budget_failed=false;
        lua_sethook(live_lua,LiveHook,LUA_MASKCOUNT,100000);
        lua_pushcfunction(live_lua,LivePredicateInvoke);
        const int result=lua_pcall(live_lua,0,1,0);
        const bool suppress=!predicate_budget_failed && result==LUA_OK && lua_isboolean(live_lua,-1) && !lua_toboolean(live_lua,-1);
        const char *diagnostic=predicate_budget_failed ? "predicate instruction budget exhausted" : result!=LUA_OK ?
            (lua_type(live_lua,-1)==LUA_TSTRING ? lua_tostring(live_lua,-1) : "non-string predicate error") :
            (!lua_isnil(live_lua,-1) && !lua_isboolean(live_lua,-1) ? "invalid predicate return type" : "");
        Debugger_PredicateResolved(suppress,diagnostic ? diagnostic : "predicate error");
        lua_settop(live_lua,top);lua_sethook(live_lua,hook,mask,count);
        predicate_active=false;
        PredicatePruneRegistrations();
        return;
    }
    if(live_failed || !live_have_line)return;
    assert(!live_active);live_active=true;Debugger_LiveBeginSlice();
    if (!live_dispatch_thread) {live_start=LiveNow();++live_slices;}
    bool ok=true;
    if(!live_lua) {
        live_lua=luaL_newstate();predicate_state=live_lua;ok=live_lua!=nullptr;
        if(ok) {
            lua_sethook(live_lua,LiveHook,LUA_MASKCOUNT,100000);
            lua_pushcfunction(live_lua,LiveInitialize);
            ok=lua_pcall(live_lua,0,0,0)==LUA_OK;
        }
    }
    if(ok) {
        lua_sethook(live_lua,LiveHook,LUA_MASKCOUNT,100000);
        if (!live_dispatch_thread) {
            lua_pushcfunction(live_lua,LivePrepareDispatch);
            ok=lua_pcall(live_lua,0,0,0)==LUA_OK;
        }
    }
    if(ok) {
        int results=0;
        lua_sethook(live_dispatch_thread,LiveHook,LUA_MASKCOUNT,100000);
        const int resumed=lua_resume(live_dispatch_thread,live_lua,0,&results);
        if (resumed==LUA_YIELD && live_phase_operation) {
            Debugger_LiveEndSlice();live_active=false;
            return; // service frame must return before native activation
        }
        ok=resumed==LUA_OK;
        if(ok) {
            lua_xmove(live_dispatch_thread,live_lua,results);
            size_t length=0;const char *text=lua_type(live_lua,-1)==LUA_TSTRING ? lua_tolstring(live_lua,-1,&length):nullptr;
            try {
                std::string reply=text?std::string(text,length)+"\n":std::string();
                ok=!reply.empty() && live_transport->reply(false,reply.data(),reply.size());
            } catch(...) {ok=false;}
        }
    }
    if(!ok) {LOG_MSG("Lua live dispatch/reply failed (transport outcome unknown)");live_failed=true;GFX_FaultLuaTasks();Debugger_LiveError();}
    if (live_dispatch_ref!=LUA_NOREF && live_lua) luaL_unref(live_lua,LUA_REGISTRYINDEX,live_dispatch_ref);
    live_dispatch_ref=LUA_NOREF;live_dispatch_thread=nullptr;
    if(live_lua)lua_settop(live_lua,0);
    live_line.clear();live_have_line=false;live_consumed=true;live_end=LiveNow();Debugger_LiveEndSlice();live_active=false;
}
// This private callback is registered with the classified machine-service owner.
// A service clears its coalesced request bit before entering us. Every early
// return (notably predicate settlement before wire dispatch) must re-arm work
// that remains; another input frame/poll is not required to make progress.
void LiveSlice(void *) {
    LiveSliceBody();
    assert(!live_active && !predicate_active && !predicate_closing);
    // Body has fully unwound Lua/predicate/close work. Never recursively dispatch
    // here: a pending predicate and a buffered line need separate safe turns.
    assert(!live_active && !predicate_active && !predicate_closing);
    if ((!live_failed && live_have_line) || Debugger_PredicatePending() ||
        close_pending || !local_load_path.empty())
        DOSBOX_RequestMachineService();
}
bool LiveFrame(const char *line,size_t length,void *) {
    if(live_consumed) {live_consumed=false;return true;}
    if(lean_protocol_closing) {live_transport->shutdown();return true;}
    if(live_have_line || !live_transport->ordinary_available()) {live_transport->defer_frame();return true;}
    live_line.assign(line,length);live_have_line=true;live_transport->defer_frame();DOSBOX_RequestMachineService();return true;
}
}
// Settle queued frames via the existing owning dispatcher after callback return.
// Native code frames only; it never interprets JSON or executes another Lua call.
void Debugger_LeanStartup() {
    static bool checked=false;
    if(checked)return;
    checked=true;
    const char *mode=getenv("DOSBOX_Y_LUA_MODE");
    if(mode && strcmp(mode,"automation-1")) E_Exit("Unknown Lua automation mode");
    NativeControl::leanMode()=mode!=nullptr;
    if(mode) {
        const char *boot=getenv("DOSBOX_Y_AUTOMATION_BOOT");
        if(boot && strcmp(boot,"run") && strcmp(boot,"hold"))
            E_Exit("Unknown Lua automation boot");
        LeanStartup();
    }
}
bool Debugger_LuaSessionPrepareDetach() {
    // Standalone evaluations close before returning. Managed/persistent states
    // have their own disposal path: ordinary slot detach cannot erase them.
    if (interactive_active || local_evaluation || local_closing || predicate_active ||
        predicate_closing || live_active || live_lua || predicate_state ||
        !local_load_path.empty() || close_pending) return false;
    // Original-engine calls retain the shared native lease until synchronous
    // borrower cleanup is complete. Do not detach during that owning call.
    if (NativeControl::owner().busy() ||
        (lean_terminal.serial && (!lean_terminal.effects.retired ||
          lean_terminal.effects.cleanup_failed || lean_terminal.effects.poisoned))) return false;
    return Debugger_LuaInteractiveReset();
}
bool Debugger_LuaPrepareRestore() {
    // Callback/close/unsupported native continuations cannot be restored.
    if(predicate_active || predicate_closing || Debugger_PredicateHeld())return false;
    PredicateReleaseRegistrations();
    // Native restore lifecycle owns the remaining Lua instrumentation teardown;
    // do not pretend an unadmitted ordinary breakpoint command succeeded.
    Debugger_PredicateResetForRestore();
    return true;
}
bool Debugger_LuaPredicateRequestPending() {return live_have_line;}
bool Debugger_LuaPredicateChannelHealthy() {
    if(persistent_local)return !live_failed;
    if(!live_transport || live_failed)return false;
    live_transport->pump(LiveNow(),LiveFrame,nullptr,nullptr);
    // First pump may only acknowledge the previously consumed deferred frame.
    // Advance framing once more before disposition, so trailing EOF is observed.
    if(!live_have_line && !live_transport->closed())
        live_transport->pump(LiveNow(),LiveFrame,nullptr,nullptr);
    const bool healthy=!live_transport->closed();
    Debugger_ExecutionMaintain(!healthy);
    if(!healthy) {
        PredicateReleaseRegistrations();
        if(live_lua){close_pending=true;DOSBOX_RequestMachineService();}
        GFX_FaultLuaTasks();
        Debugger_LiveError();
    }
    return healthy;
}
bool Debugger_LuaCheckpointOwnerMustDispose(uint64_t token) {
    return (checkpoint_restored && token==checkpoint_owner) || (token && token==lean_loaded_owner);
}
bool Debugger_LuaCheckpointPoisoned(uint64_t token) {return token && token==lean_poison_owner;}
void Debugger_LuaCheckpointOwnerDeparting(uint64_t token) {
    if(!Debugger_LuaCheckpointOwnerMustDispose(token))return;
    // Native execution has retired. No guest or transport dispatch at this fence.
    assert(!live_active && !predicate_active && !predicate_closing);
    LeanSettle("checkpoint_owner_departed");
    live_failed=true;live_have_line=false;
    live_dispatch_thread=nullptr;live_dispatch_ref=LUA_NOREF;live_phase_operation=0;
    PredicateReleaseRegistrations();
    if(live_lua) {
        predicate_closing=true;lua_close(live_lua);predicate_closing=false;
        live_lua=nullptr;predicate_state=nullptr;
    }
    close_pending=false;
    if(DEBUG_DetachNativeExecution(live_attachment)){live_attachment={};local_attachment={};}
    live_checkpoints.clear();
    fprintf(stderr,"Lua checkpoint: restored owner disposed before native return (epoch=%llu); persistent state closed\n",
            (unsigned long long)checkpoint_epoch);
}
// Called only after the installing service has unwound, before frame disposal.
// No guest loop/device/input servicing occurs in this bounded host-only phase.
bool Debugger_LuaCheckpointDrainPending(uint64_t token) {
    static uint64_t owner=0,start=0;
    if(!Debugger_LuaCheckpointPoisoned(token))return false;
    if(owner!=token){owner=token;start=LiveNow();}
    return live_transport && !live_failed && !live_transport->closed() &&
        LiveNow()-start<1000 &&
        !(lean_fatal_observed && live_transport->ordinary_available());
}
bool Debugger_LuaLiveConfigured() {return live_transport && !live_failed;}
bool Debugger_LuaInLiveService() {return live_active;}
void Debugger_StateOwnerDeparting(uint64_t token) {
    // Synchronous native calls own borrowers until return; departure revokes
    // authority without discarding retained terminal effect evidence.
    if(token && token==DEBUG_NativeExecutionContext().invocation)
        lean_terminal.effects.usable=false;
}
void Debugger_LuaLivePump() {
    EnsureDebuggerService();
    if(!live_checked) {
        live_checked=true;
        Debugger_LeanStartup();
        live_module=getenv("DOSBOX_Y_LUA_LIVE_MODULE");
        const char *input=getenv("DOSBOX_Y_LUA_INPUT_FD"),*output=getenv("DOSBOX_Y_LUA_OUTPUT_FD");
        if(!live_module || !input || !output) {
            if(NativeControl::leanMode()) E_Exit("Missing lean live module/descriptors");
            return;
        }
        if(persistent_local || !local_load_path.empty()) {
            fprintf(stderr,"Lua debugger: live transport conflicts with persistent local state\n");
            live_failed=true;return;
        }
        if(NativeControl::leanMode() && (!*input || !*output ||
           strspn(input,"0123456789")!=strlen(input) || strspn(output,"0123456789")!=strlen(output)))
            E_Exit("Invalid lean decimal descriptor");
        char *end=nullptr;long in=strtol(input,&end,10);if(!*input || *end || in<0 || in>INT_MAX) {
            if(NativeControl::leanMode()) E_Exit("Invalid lean input descriptor");
            return;
        }
        long out=strtol(output,&end,10);if(!*output || *end || out<0 || out>INT_MAX || in==out) {
            if(NativeControl::leanMode()) E_Exit("Invalid lean output descriptor");
            return;
        }
        const char *root=getenv("DOSBOX_Y_LUA_ROOT_FD"),*limit=getenv("DOSBOX_Y_LUA_LIMIT");
        const char *state_fd=getenv("DOSBOX_AUTOMATION_STATE_FD");
        const char *state_access=getenv("DOSBOX_AUTOMATION_STATE_ACCESS");
        const char *state_bytes=getenv("DOSBOX_AUTOMATION_STATE_BYTES");
        uint64_t capture_fd=0;
        const bool capture_number=NativeInput::decimal(root,INT_MAX,capture_fd);
        if(!NativeControl::leanMode() && !live_state_root.admit(state_fd,state_access,state_bytes,int(in),int(out),
                                  capture_number ? int(capture_fd) : -1))
            E_Exit("Invalid automation state-root startup authority");
        if(!NativeControl::leanMode() && (root || limit)) {
            NativeInput::Startup settings;
            if(!NativeInput::startup(input,output,root,limit,"lua",settings) || !GFX_InstallLuaTasks(settings.root,settings.limit)) {
                live_state_root.closeRoot();
                return;
            }
        }
        if(NativeControl::leanMode()) {
            live_attachment=DEBUG_AttachNativeExecution(NativeDebug::Frontend::Lua,0);
            if (!live_attachment.serial)
                E_Exit("Lua live carrier conflicts with selected debugger session");
        }
        live_transport.reset(new NativeInput::LiveTransport(int(in),int(out)));
        if(NativeControl::leanMode()) {
            if(live_module[0]!='/' || live_transport->closed()) E_Exit("Invalid lean carrier/module");
            live_lua=luaL_newstate();
            if(!live_lua) E_Exit("Lean Lua allocation failed");
            lua_pushcfunction(live_lua,LiveInitializeLean);
            if(lua_pcall(live_lua,0,0,0)!=LUA_OK)
                E_Exit("Lean module initialization failed");
        }
    }
    // A poisoned frame may only move transport bytes and accept terminal
    // queries before disposal. Do not run entry/predicate/task maintenance:
    // those paths can deliver queued native input or touch output resources.
    if(Debugger_LuaCheckpointPoisoned(DOSBOX_MachineServiceOwner().token)) {
        if(live_transport && !live_failed && !live_transport->closed())
            live_transport->pump(LiveNow(),LiveFrame,nullptr,nullptr);
        return;
    }
    if(Debugger_PredicateHeld())return;
    Debugger_EntryMaintain();
    if(live_transport && !live_failed && Debugger_LiveEffectiveStopped() && !strcmp(Debugger_LiveStopReason(),"genuine"))Debugger_LiveGenuineStop();
    if(live_transport && !live_failed && !live_transport->closed())live_transport->pump(LiveNow(),LiveFrame,nullptr,nullptr);
    if(lean_protocol_closing && live_transport && live_transport->ordinary_available())
        live_transport->shutdown();
    if(live_transport && (live_failed || live_transport->closed())) {
        LeanSettle("channel_lost");
        // Entry hold is authority to retain a fresh destination, not permission
        // to outlive its carrier. Dispose the current owned frame on EOF/fault;
        // never resume a partially installed or pre-entry guest after loss.
        if(entry_hold.lean && entry_hold.launch) {
            entry_hold.armed=false;entry_hold.expired=true;
            lean_poison_owner=DOSBOX_MachineServiceOwner().token;
        }
        PredicateReleaseRegistrations();
        // Native settlement above closes archive borrowers before this root;
        // it is independent of Lua's lifetime or projected session state.
        live_state_root.closeRoot();
        if(live_lua && !close_pending){close_pending=true;DOSBOX_RequestMachineService();}
        else if(!live_lua && !predicate_active && !live_active) {
            if (DEBUG_DetachNativeExecution(live_attachment)) {live_attachment={};local_attachment={};}
        }
        if(!live_channel_reported) {
            live_channel_reported=true;
            LOG_MSG("Lua live channel closed: transport_fault=%u dispatch_failed=%u",unsigned(live_transport->fault()),unsigned(live_failed));
        }
        GFX_FaultLuaTasks();
    }
    Debugger_ExecutionMaintain(live_failed || (live_transport && live_transport->closed()));
    const auto &target=Debugger_ExecutionTarget();
    const bool matched_stop=!strcmp(target.reason,"hit") &&
        Debugger_LiveOwnsStop(target.owner);
    GFX_PumpLuaTasks(!Debugger_LiveEffectiveStopped(),matched_stop);
}

void Debugger_EntryLaunchHold() {
 static bool checked=false;
 const auto owner=DOSBOX_MachineServiceOwner();
 if(checked || owner.kind!=MachineService::Kind::RootGuest || owner.depth!=1)return;
 checked=true;
 const char *setting=getenv("DOSBOX_Y_LUA_LAUNCH_HOLD_MS");
 if(!setting && !getenv("DOSBOX_Y_LUA_MODE"))return;
 if(getenv("DOSBOX_Y_LUA_MODE")) {
  const char *boot=getenv("DOSBOX_Y_AUTOMATION_BOOT");
  if(!boot || !strcmp(boot,"run"))return;
  Debugger_LuaLivePump();
  if(!Debugger_LuaLiveConfigured() || !Debugger_LiveStop(owner.token)) E_Exit("Lean root hold failed");
  return;
 }
 char *end=nullptr;long duration=strtol(setting,&end,10);
 if(!*setting || *end || duration<1 || duration>10000) {LOG_MSG("Invalid entry launch hold; terminating fresh launch");throw 1;}
 Debugger_LuaLivePump();
 LOG_MSG("Entry launch admission configured=%u stopped=%u depth=%u denial=%u",unsigned(Debugger_LuaLiveConfigured()),unsigned(Debugger_LiveEffectiveStopped()),owner.depth,unsigned(DOSBOX_MachineServiceAdmission()));
 if(!Debugger_LuaLiveConfigured() || Debugger_LiveEffectiveStopped() ||
    DOSBOX_MachineServiceAdmission()!=MachineService::Denial::None) {
  LOG_MSG("Entry launch hold denied; terminating fresh launch");throw 1;
 }
 entry_hold.launch=true;entry_hold.owner=owner.token;entry_hold.deadline=LiveNow()+duration;
 Debugger_LiveStop(owner.token);
}
void Debugger_EntryMaintain() {
 if(!entry_hold.launch || entry_hold.expired)return;
 // Startup load is synchronous on the admitted native frame. There is no
 // separate pending-load job whose deadline can extend this entry hold.
 if(LiveNow()>=entry_hold.deadline) {
  entry_hold.armed=false;entry_hold.expired=true;
  if(entry_hold.lean) {
   // Nonarrival can expire while a different naturally launched child is
   // current. Poison that active frame, not the suspended root token: otherwise
   // an infinite wrong child could keep the expired session executing forever.
   lean_poison_owner=DOSBOX_MachineServiceOwner().token;
  }
  // Fail closed. External finite session owner disposes; no implicit resume.
  if(entry_hold.owner==DOSBOX_MachineServiceOwner().token)
   Debugger_LiveStop(entry_hold.owner);
 }
}
bool Debugger_LocalDriverArm(NativeDebug::Attachment attachment,
                             NativeDebug::Context context,const std::string &program) {
    const auto owner=DOSBOX_MachineServiceOwner();
    // Validate the root attachment before changing the selected stopped state.
    // Startup arrives at an audited native hold; it need not already be a UI stop.
    if (!DEBUG_AdmitNativeState(attachment,context,nullptr,false) ||
        owner.kind != MachineService::Kind::RootGuest || owner.depth != 1 ||
        entry_hold.generation || entry_hold.launch || NativeControl::owner().busy() ||
        Debugger_PredicateHeld() || PredicateEnabled() ||
        !LuaDriver::Options::canonicalProgram(program)) return false;
    const auto tasks = GFX_LuaTaskStatus();
    if (tasks.pending || tasks.held) return false;
    EntryHold prepared;
    prepared.program = program;
    std::replace(prepared.program.begin()+2,prepared.program.end(),'/','\\');
    prepared.launch = prepared.armed = prepared.lean = true;
    prepared.owner = owner.token;
    prepared.generation = 1;
    prepared.deadline = LiveNow()+30000;
    prepared.hold_ms = 120000;
    if (!Debugger_LiveStop(owner.token)) return false;
    entry_hold=std::move(prepared);
    return Debugger_LiveResume(owner.token);
}
int Debugger_LocalDriverEntry() {
    Debugger_EntryMaintain();
    if(entry_hold.expired)return -1;
    if(!entry_hold.hit)return 0;
    const auto owner=DOSBOX_MachineServiceOwner();
    const bool receipt = owner.kind == MachineService::Kind::ShellExecGuest &&
        owner.depth == 2 && owner.token == natural_executable.owner &&
        natural_executable.psp && natural_executable.psp == dos.psp() &&
        !natural_executable.executable.empty() &&
        natural_executable.executable == entry_hold.executable &&
        Debugger_LiveOwnsStop(owner.token);
    return owner.token==entry_hold.owner && receipt?1:0;
}
void Debugger_LocalDriverStateReady(NativeDebug::Attachment attachment,
                                   NativeDebug::Context context,LuaDriver::Response &response) {
    // This projects admission for the shared original-engine operation, not a
    // second device catalogue or a promise that a subsequent serialization
    // cannot fail. Save revalidates the exact context and takes its own lease.
    const auto owner = DOSBOX_MachineServiceOwner();
    if (!DEBUG_AdmitNativeState(attachment,context)) {
        response.error = "state operation requires current owned stopped context";
        return;
    }
    response.state_profile = "original-component-stream";
    if (owner.kind != MachineService::Kind::ShellExecGuest || owner.depth != 2)
        response.state_reasons.emplace_back("state operation requires current shell-exec guest");
    if (!NativeFrontier::frontiers().quiescent())
        response.state_reasons.emplace_back("state operation requires unwound native continuation");
}
void Debugger_LocalDriverSave(NativeDebug::Attachment attachment,
                             NativeDebug::Context context,const std::string &path,
                             uint32_t timeout,LuaDriver::SaveEffects &effects) {
    // Cooperative budget at original owner boundaries; individual blocking I/O
    // and serializers must settle before close, without a competing pump.
    const StateArchive::Budget budget(timeout);
    effects = {};
    effects.valid = true;
    StateArchive::Effects archive;
    // Reserve either terminal code before the serializer can publish. A later
    // response allocation failure must not erase the only copied effect facts.
    effects.code.reserve(sizeof("save_refused_or_failed")-1);
    try {
        effects.completed = DEBUG_NativeStateFile(attachment,context,path,false,&archive,&budget);
    } catch (const std::exception &) {
        // Save does not install guest state. The shared helper unwinds its
        // lease and ZIP/FILE owners before these terminal facts are copied.
        effects.completed = false;
    } catch (const SaveState::Error &) {
        effects.completed = false;
    }
    effects.published = archive.published;
    effects.cleanup_attempted = archive.cleanup_attempted;
    effects.cleanup_failed = archive.cleanup_failed;
    // Returning from the shared operation means the exact lease is released.
    effects.retired = true;
    effects.usable = !archive.cleanup_failed &&
        DEBUG_AdmitNativeState(attachment,context);
    effects.code = effects.completed ? "saved" :
        (archive.timed_out ? "timeout" : "save_refused_or_failed");
}
const char *Debugger_LocalDriverLoad(NativeDebug::Attachment attachment,
                                    NativeDebug::Context context,const std::string &archive) {
    // Startup runs on the admitted held native frame before the worker exists.
    // The shared operation owns the lease, original component-stream install,
    // fatal partial-install handling, and retirement of the previous context.
    // Do not pump host events or a second state-job execution loop here.
    return DEBUG_NativeStateFile(attachment,context,archive,true) ?
        nullptr : "startup state load refused or failed";
}
void Debugger_EntryLoaded(const char *name,uint16_t psp,uint16_t load,uint16_t cs,uint16_t ip,const char *executable) {
 // Natural identity recording neither arms a hold nor changes execution.
 RecordNaturalExecutable(executable,psp);
 if(!entry_hold.armed)return;
 Debugger_EntryMaintain();if(!entry_hold.armed)return;
 const auto owner=DOSBOX_MachineServiceOwner();
 LOG_MSG("Entry candidate name=%s expected=%s executable=%s depth=%u denial=%u",name,entry_hold.program.c_str(),executable?executable:"",owner.depth,unsigned(DOSBOX_MachineServiceAdmission()));
 if(owner.kind!=MachineService::Kind::ShellExecGuest || owner.depth!=2 ||
    DOSBOX_MachineServiceAdmission()!=MachineService::Denial::None ||
    (entry_hold.lean ? (!executable || entry_hold.program!=executable) :
                       strcasecmp(name,entry_hold.program.c_str())))return;
 // After the native handler returns Normal_Loop observes exitLoop before
 // another PIC/decoder iteration. Registers still point at DOS RETF.
 // Receipt preparation precedes stop/hold mutation. Failure is optional
 // metadata loss, not permission to unwind the DOS EXEC with open resources.
 std::string executable_receipt;
 try { executable_receipt=executable ? executable : ""; }
 catch (...) { return; }
 if(!Debugger_LiveStop(owner.token))return;
 CPU_CycleLeft+=CPU_Cycles;CPU_Cycles=0;extern bool exitLoop;exitLoop=true;
 entry_hold.executable.swap(executable_receipt);
 entry_hold.owner=owner.token;entry_hold.psp=psp;entry_hold.load=load;
 entry_hold.cs=cs;entry_hold.ip=ip;entry_hold.armed=false;entry_hold.hit=true;
 if(entry_hold.lean){entry_hold.deadline=LiveNow()+entry_hold.hold_ms;entry_hold.load_eligible=true;}
}

static int InteractiveOutput(lua_State *L)
{
    for (int i=1;i<=lua_gettop(L);++i) {
        size_t length=0;
        const char *text=luaL_tolstring(L,i,&length);
        DEBUG_ShowMsg("%s%.*s",i==1 ? "" : "\t",int(length>2000?2000:length),text);
        lua_pop(L,1);
    }
    return 0;
}
static int InteractiveNoExecution(lua_State *L)
{
    return luaL_error(L,"Interactive Lua cannot execute guest code or retain callbacks; use native RUN/STEPIN after evaluation");
}
static int InteractiveInitialize(lua_State *L)
{
    luaL_openlibs(L);
    PredicateInstallLoadGuards(L);
    Debugger_LuaRegisterDbg(L);
    lua_pushcfunction(L,InteractiveOutput);
    lua_setglobal(L,"print");
    lua_getglobal(L,"dbg");
    for(const char *name:{"step","run_until_event","request_step","on_break"}) {
        lua_pushcfunction(L,InteractiveNoExecution);
        lua_setfield(L,-2,name);
    }
    return 0;
}
struct InteractiveEvaluation {
    const char *input;
    const std::string *expression;
    bool file;
};
static int InteractiveEvaluate(lua_State *L)
{
    const auto &evaluation=*static_cast<InteractiveEvaluation *>(lua_touserdata(L,1));
    lua_settop(L,0);
    int result;
    if(evaluation.file)result=luaL_loadfile(L,evaluation.input);
    else {
        result=luaL_loadbuffer(L,evaluation.expression->data(),evaluation.expression->size(),"=Interactive");
        if(result!=LUA_OK) {
            // Only a syntax error selects the statement form. In particular,
            // do not turn an allocation failure into a second load attempt.
            if(result!=LUA_ERRSYNTAX)return lua_error(L);
            lua_pop(L,1);
            result=luaL_loadbuffer(L,evaluation.input,strlen(evaluation.input),"=Interactive");
        }
    }
    if(result!=LUA_OK)return lua_error(L);
    lua_call(L,0,LUA_MULTRET);
    const int count=lua_gettop(L);
    if(count) {
        lua_pushcfunction(L,InteractiveOutput);
        lua_insert(L,1);
        lua_call(L,count,0);
    }
    return 0;
}
bool Debugger_LuaInteractiveReset()
{
    if(interactive_active)return false;
    if(interactive_lua) {
        // Finalizers cannot issue guest effects while the state is closing.
        const bool previous=predicate_closing;
        predicate_closing=true;
        lua_close(interactive_lua);
        predicate_closing=previous;
        interactive_lua=nullptr;
    }
    interactive_attachment={};
    return true;
}
bool Debugger_LuaInteractive(const char *input, bool file, NativeDebug::Attachment attachment)
{
    if(!input || !*input || interactive_active || local_evaluation || live_lua ||
       predicate_state || predicate_active || predicate_closing ||
       !DEBUG_AdmitNativeEffect(attachment) ||
       DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0).serial!=attachment.serial) {
        DEBUG_ShowMsg("Lua: evaluation refused (requires selected stopped Interactive session)");
        return false;
    }
    if(interactive_lua && interactive_attachment.serial!=attachment.serial)
        Debugger_LuaInteractiveReset();
    if(!interactive_lua) {
        interactive_lua=luaL_newstate();
        if(!interactive_lua)return false;
        interactive_attachment=attachment;
        lua_pushcfunction(interactive_lua,InteractiveInitialize);
        if(lua_pcall(interactive_lua,0,0,0)!=LUA_OK) {
            DEBUG_ShowMsg("Lua: initialization failed: %s",
                lua_type(interactive_lua,-1)==LUA_TSTRING ? lua_tostring(interactive_lua,-1) : "non-string error");
            Debugger_LuaInteractiveReset();
            return false;
        }
    }
    // Prepare C++ storage before selecting evaluation state. All Lua loading,
    // including file readers and result formatting, runs beneath one protected
    // call so an allocation error cannot leave interactive_active latched.
    std::string expression;
    try {if(!file)expression=std::string("return ")+input;}
    catch(const std::exception &) {
        DEBUG_ShowMsg("Lua: expression allocation failed");
        return false;
    }
    InteractiveEvaluation evaluation{input,&expression,file};
    const auto previous=local_attachment;
    local_attachment=attachment;
    interactive_active=true;
    lua_State *L=interactive_lua;
    lua_pushcfunction(L,InteractiveEvaluate);
    lua_pushlightuserdata(L,&evaluation);
    const int result=lua_pcall(L,1,0,0);
    if(result!=LUA_OK)
        DEBUG_ShowMsg("Lua: %s",lua_type(L,-1)==LUA_TSTRING?lua_tostring(L,-1):"non-string error");
    lua_settop(L,0);
    interactive_active=false;
    local_attachment=previous;
    return result==LUA_OK;
}

static int LocalEvaluateFile(lua_State *L)
{
    const char *path=static_cast<const char *>(lua_touserdata(L,1));
    lua_settop(L,0);
    luaL_openlibs(L);
    PredicateInstallLoadGuards(L);
    Debugger_LuaRegisterDbg(L);
    if(luaL_loadfile(L,path)!=LUA_OK)return lua_error(L);
    lua_call(L,0,0);
    return 0;
}

bool Debugger_LuaRunScriptInStoppedState(const char *script_path)
{
    if(!NativeControl::diagnosticEffectAllowed())return false;
	if (script_path == NULL || script_path[0] == '\0') {
		LOG_MSG("Lua debugger: no script path provided");
		return false;
	}

	const DebuggerStatus status = Debugger_GetStatus();
	if (!status.stopped) {
		LOG_MSG("Lua debugger: refusing to run '%s' outside a debugger-safe stopped state", script_path ? script_path : "(null)");
		return false;
	}

    if(local_evaluation || predicate_active || predicate_closing || live_active || Debugger_PredicateHeld()) {
        fprintf(stderr,"Lua debugger: nested script loading unavailable\n");return false;
    }
    const char *local_file=getenv("DOSBOX_Y_LUA_PERSISTENT_LOCAL");
    const bool persistent=local_file && *local_file && !strcmp(local_file,script_path);
    if(persistent) {
        if(live_lua || !local_load_path.empty() || getenv("DOSBOX_Y_LUA_LIVE_MODULE") ||
           (DOSBOX_MachineServiceAdmission()!=MachineService::Denial::None &&
            DOSBOX_MachineServiceAdmission()!=MachineService::Denial::NoOwner)) {
            fprintf(stderr,"Lua debugger: persistent local load conflicts or is unadmitted\n");return false;
        }
        EnsureDebuggerService();
        local_load_path=script_path;
        DOSBOX_RequestMachineService();
        return true;
    }
    local_attachment=DEBUG_AttachNativeExecution(NativeDebug::Frontend::Interactive,0);
    if (!local_attachment.serial) {
        LOG_MSG("Lua debugger: another debugger session is attached");
        return false;
    }
	lua_State *L = luaL_newstate();
	if (L == NULL) {
		LOG_MSG("Lua debugger: failed to create Lua state");
		return false;
	}

    local_evaluation=true;local_intent=false;
    lua_pushcfunction(L,LocalEvaluateFile);
    lua_pushlightuserdata(L,const_cast<char *>(script_path));
    const bool ok = lua_pcall(L,1,0,0) == LUA_OK;
	if (!ok)
		LOG_MSG("Lua debugger: error running '%s': %s", script_path,
            lua_type(L,-1)==LUA_TSTRING ? lua_tostring(L,-1) : "non-string error");

    local_closing=true;
	predicate_closing=true;lua_close(L);predicate_closing=false;
    local_closing=false;
    local_evaluation=false;
    const bool commit=ok && local_intent;
    local_intent=false;
    if (commit && DEBUG_RequestNativeExecution(local_request)!=NativeDebug::Admission::Accepted) {
        LOG_MSG("Lua debugger: execution intent admission refused; no execution committed");
        return false;
    }
    return ok;
}

#endif