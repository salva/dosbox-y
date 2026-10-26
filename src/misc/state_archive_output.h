/* GPL-2.0-or-later. Retained native output for original-owner ZIP archives. */
#ifndef DOSBOX_STATE_ARCHIVE_OUTPUT_H
#define DOSBOX_STATE_ARCHIVE_OUTPUT_H

#include <cerrno>
#include <cstdio>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "ioapi.h"

// One archive, not a transaction spanning the state and flagged-file sidecar.
// Minizip borrows the reserved FILE directly, never reopening its pathname.
class StateArchiveOutput {
    std::string destination_, temporary_;
    FILE *file_ = nullptr;
    bool borrowed_ = false;
    bool finalized_ = false;

    static voidpf ZCALLBACK open(voidpf opaque, const void *, int) {
        auto &self = *static_cast<StateArchiveOutput *>(opaque);
        if (self.borrowed_ || !self.file_) return nullptr;
        self.borrowed_ = true;
        return self.file_;
    }
    static int ZCALLBACK close(voidpf opaque, voidpf stream) {
        auto &self = *static_cast<StateArchiveOutput *>(opaque);
        FILE *file = static_cast<FILE *>(stream);
        bool ok = fflush(file) == 0;
#ifdef WIN32
        if (_commit(_fileno(file)) != 0) ok = false;
#else
        if (fsync(fileno(file)) != 0) ok = false;
#endif
        if (fclose(file) != 0) ok = false;
        self.file_ = nullptr; // Even failed fclose consumes the resource.
        self.finalized_ = ok;
        return ok ? 0 : -1;
    }
public:
    explicit StateArchiveOutput(const std::string &destination)
            : destination_(destination) {}
    StateArchiveOutput(const StateArchiveOutput &) = delete;
    StateArchiveOutput &operator=(const StateArchiveOutput &) = delete;
    ~StateArchiveOutput() {
        if (file_) fclose(file_);
        if (!temporary_.empty() && std::remove(temporary_.c_str()) != 0)
            fprintf(stderr, "Save output cleanup failed: %s (errno %d)\n",
                    temporary_.c_str(), errno);
    }
    bool prepare(zlib_filefunc64_def &io) {
        if (file_ || !temporary_.empty()) return false;
        static unsigned long sequence = 0; // emulator thread only
        for (unsigned attempt = 0; attempt < 128; ++attempt) {
#ifdef WIN32
            const auto pid = _getpid();
#else
            const auto pid = getpid();
#endif
            const std::string path = destination_ + ".tmp-" +
                std::to_string(pid) + "-" + std::to_string(++sequence);
#ifdef WIN32
            const int fd = _open(path.c_str(), _O_CREAT | _O_EXCL | _O_RDWR |
                                 _O_BINARY, _S_IREAD | _S_IWRITE);
#else
            const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
#endif
            if (fd < 0) {
                if (errno == EEXIST) continue;
                return false;
            }
            temporary_ = path;
#ifdef WIN32
            file_ = _fdopen(fd, "w+b");
            if (!file_) _close(fd);
#else
            file_ = fdopen(fd, "w+b");
            if (!file_) ::close(fd);
#endif
            if (!file_) return false;
            fill_fopen64_filefunc(&io);
            io.opaque = this;
            io.zopen64_file = open;
            io.zclose_file = close;
            return true;
        }
        return false;
    }
    bool publish() {
        if (!borrowed_ || file_ || !finalized_ || temporary_.empty()) return false;
#ifdef WIN32
        if (!MoveFileExA(temporary_.c_str(), destination_.c_str(),
                         MOVEFILE_REPLACE_EXISTING)) return false;
#else
        if (std::rename(temporary_.c_str(), destination_.c_str()) != 0) return false;
#endif
        temporary_.clear();
        return true;
    }
};
#endif
