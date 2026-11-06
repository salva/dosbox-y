#ifndef DOSBOX_VGA_CURSOR_COLOR_H
#define DOSBOX_VGA_CURSOR_COLOR_H
#include <cstring>
// Native-endian cursor color semantics, without assuming byte-array alignment.
template<class T> inline T VGA_CursorColor(const unsigned char *bytes) {
    T value;std::memcpy(&value,bytes,sizeof(value));return value;
}
#endif
