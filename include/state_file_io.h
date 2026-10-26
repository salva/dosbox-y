// Host-side state file transfer is not guest disk activity.
#ifndef DOSBOX_STATE_FILE_IO_H
#define DOSBOX_STATE_FILE_IO_H
namespace StateFileIO {
inline unsigned &depth() { static unsigned value = 0; return value; }
inline bool active() { return depth() != 0; }
class Scope {
public:
    Scope() { ++depth(); }
    ~Scope() { --depth(); }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
};
}
#endif
