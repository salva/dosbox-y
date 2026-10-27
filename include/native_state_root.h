/* GPL-2.0-or-later. Separately transferred automation state directory. */
#ifndef DOSBOX_NATIVE_STATE_ROOT_H
#define DOSBOX_NATIVE_STATE_ROOT_H
#include "native_startup.h"
#include <initializer_list>
namespace NativeInput {
class StateRoot {
    int fd_ = -1;
    bool writable_ = false;
    size_t bytes_ = 0;
    bool admitted_ = false;
public:
    StateRoot() = default;
    StateRoot(const StateRoot &) = delete;
    StateRoot &operator=(const StateRoot &) = delete;
    ~StateRoot() { closeRoot(); }
    void closeRoot() {
        if (fd_ >= 0) close(fd_);
        fd_ = -1;
        writable_ = false;
        bytes_ = 0;
        // Closing never permits rebinding this process-instance owner.
    }
    int fd() const { return fd_; }
    bool writable() const { return writable_; }
    size_t bytes() const { return bytes_; }
    // The caller settles all borrowing jobs before closeRoot/destruction.
    // A syntactically identified, distinct transferred FD is consumed on failure
    // too. Aliases of transport/capture authority are NOT ours to close.
    bool admit(const char *number, const char *access, const char *budget,
               int input, int output, int capture) {
        if (admitted_) return false;
        admitted_ = true;
        if (!number && !access && !budget) return true;
        uint64_t raw = 0, limit = 0;
        if (!decimal(number, INT_MAX, raw) || raw < 3) return false;
        const int transferred = int(raw);
        if (transferred == input || transferred == output || transferred == capture)
            return false;
        struct Transfer {
            int fd;
            ~Transfer() { close(fd); }
        } transfer{transferred};
        if (!access || (strcmp(access, "read") && strcmp(access, "read_write")) ||
            !decimal(budget, 67108864, limit) || !limit) return false;
        const int flags = fcntl(transferred, F_GETFL);
        struct stat state, other;
        if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
#ifdef O_PATH
            (flags & O_PATH) ||
#endif
            fstat(transferred, &state) || !S_ISDIR(state.st_mode) ||
            state.st_uid != geteuid() || (state.st_mode & 0022) ||
            !(state.st_mode & S_IRUSR) || !(state.st_mode & S_IXUSR)) return false;
        for (const int fd : {input, output, capture}) {
            if (fd >= 0 && !fstat(fd, &other) && state.st_dev == other.st_dev &&
                state.st_ino == other.st_ino) return false;
        }
        const bool write = !strcmp(access, "read_write");
        if (write && !(state.st_mode & S_IWUSR)) return false;
        // faccessat retains OS/ACL enforcement, including when the test runs root.
        if (faccessat(transferred, ".", R_OK | X_OK | (write ? W_OK : 0), AT_EACCESS))
            return false;
        fd_ = fcntl(transferred, F_DUPFD_CLOEXEC, 3);
        if (fd_ < 0) return false;
        writable_ = write;
        bytes_ = size_t(limit);
        return true;
    }
};
}
#endif
