/* GPL-2.0-or-later. Bounded initial input vocabulary and integer viewport map. */
#ifndef DOSBOX_NATIVE_INPUT_H
#define DOSBOX_NATIVE_INPUT_H
#include <stddef.h>
#include "keyboard.h"
#include <stdint.h>
#include <string.h>
#include <limits.h>
namespace NativeInput {
// One vocabulary for admission, ownership, cleanup and receipts. Existing key
// ordinals remain stable; buttons always follow the complete key domain.
struct KeyEntry { const char *name; KBD_KEYS value; };
static constexpr KeyEntry KeyVocabulary[] = {
    {"KBD_a",KBD_a},{"KBD_enter",KBD_enter},{"KBD_space",KBD_space},
    {"KBD_esc",KBD_esc},{"KBD_left",KBD_left},{"KBD_right",KBD_right},
    {"KBD_up",KBD_up},{"KBD_down",KBD_down},
    {"KBD_0",KBD_0},{"KBD_1",KBD_1},{"KBD_2",KBD_2},
    {"KBD_3",KBD_3},{"KBD_4",KBD_4},{"KBD_5",KBD_5},
    {"KBD_6",KBD_6},{"KBD_7",KBD_7},{"KBD_8",KBD_8},
    {"KBD_9",KBD_9},{"KBD_backspace",KBD_backspace},
    {"KBD_leftshift",KBD_leftshift},
    {"KBD_f1",KBD_f1},{"KBD_f2",KBD_f2},{"KBD_f3",KBD_f3},
    {"KBD_f4",KBD_f4},{"KBD_f5",KBD_f5},{"KBD_f6",KBD_f6}
};
static constexpr unsigned KeyCount = sizeof(KeyVocabulary)/sizeof(KeyVocabulary[0]);
static constexpr unsigned EdgeCount = KeyCount + 3;
inline bool key_index(const char *name, unsigned &index) {
    if (!name) return false;
    for (unsigned i=0;i<KeyCount;i++) if (!strcmp(name,KeyVocabulary[i].name)) {
        index=i;return true;
    }
    return false;
}
inline bool key(const char *name, KBD_KEYS &value) {
    unsigned index;
    if (!key_index(name,index)) return false;
    value=KeyVocabulary[index].value;return true;
}
struct Point { int32_t x=0,y=0,dx=0,dy=0; };
class Cursor {
    bool valid_ = false;
    int32_t x_ = 0, y_ = 0;
public:
    void invalidate() {valid_=false;}
    // Validation is pure: cursor advances only after shared primitive dispatch.
    bool map(uint32_t x,uint32_t y,int32_t origin_x,int32_t origin_y,
             uint32_t width,uint32_t height,Point &out) const {
        if (x>65535 || y>65535 || origin_x<0 || origin_y<0 ||
            width<2 || width>4096 || height<2 || height>4096) return false;
        int64_t px=int64_t((uint64_t(x)*(width-1)+32767)/65535);
        int64_t py=int64_t((uint64_t(y)*(height-1)+32767)/65535);
        if (px+origin_x>INT32_MAX || py+origin_y>INT32_MAX) return false;
        int64_t dx=valid_ ? px-x_ : 0, dy=valid_ ? py-y_ : 0;
        if (dx < -4095 || dx > 4095 || dy < -4095 || dy > 4095) return false;
        out.x=int32_t(px+origin_x);out.y=int32_t(py+origin_y);
        out.dx=int32_t(dx);out.dy=int32_t(dy);return true;
    }
    void dispatched(const Point &p,int32_t origin_x,int32_t origin_y) {
        x_=p.x-origin_x;y_=p.y-origin_y;valid_=true;
    }
};
}
#endif
