/* GPL-2.0-or-later. Conservative selected native input profile snapshot. */
#ifndef DOSBOX_NATIVE_PROFILE_H
#define DOSBOX_NATIVE_PROFILE_H
#include "native_input.h"
namespace NativeInput {
struct Profile {
    bool windowed=false,ibm_vga=false,unlocked_never=false,keyboard=false,mouse=false;
    bool sensitivity=false,notify_off=false,route_geometry=false;
    bool pixel_geometry=false;
    // Actual tuple values, not just supported/unsupported booleans. Changes
    // among unsupported profiles must also advance generation.
    uint64_t keyboard_tuple=0,mouse_tuple=0,mode_identity=0;
    uint32_t mode_type=0,mode_width=0,mode_height=0,machine_id=0;
    int32_t drawable_width=0,drawable_height=0,lock=0,emulation=0;
    int32_t xsensitivity=0,ysensitivity=0;
    int32_t min_x=0,max_x=0,min_y=0,max_y=0,screen_x=0,screen_y=0;
    uint32_t notify=0;
    bool user_locked=false;
    int32_t user_emulation=0,adjust_x=0,adjust_y=0;
    struct {int32_t x=0,y=0;uint32_t width=0,height=0;} viewport;
    int32_t user_width=0,user_height=0;
    bool supported() const {
        Point point;
        return windowed && ibm_vga && unlocked_never && keyboard && mouse &&
            sensitivity && notify_off && route_geometry && pixel_geometry && user_width>=2 && user_width<=4096 &&
            user_height>=2 && user_height<=4096 &&
            user_width==int64_t(viewport.width) && user_height==int64_t(viewport.height) &&
            drawable_width>=2 && drawable_height>=2 &&
            int64_t(viewport.x)+viewport.width<=drawable_width &&
            int64_t(viewport.y)+viewport.height<=drawable_height && Cursor().map(65535,65535,viewport.x,viewport.y,viewport.width,viewport.height,point);
    }
    bool same(const Profile &b) const {
        return min_x==b.min_x && max_x==b.max_x && min_y==b.min_y && max_y==b.max_y &&
            screen_x==b.screen_x && screen_y==b.screen_y && keyboard_tuple==b.keyboard_tuple && mouse_tuple==b.mouse_tuple &&
            mode_identity==b.mode_identity && mode_type==b.mode_type &&
            mode_width==b.mode_width && mode_height==b.mode_height && machine_id==b.machine_id &&
            drawable_width==b.drawable_width && drawable_height==b.drawable_height &&
            lock==b.lock && emulation==b.emulation && xsensitivity==b.xsensitivity &&
            ysensitivity==b.ysensitivity && notify==b.notify && user_locked==b.user_locked &&
            user_emulation==b.user_emulation && adjust_x==b.adjust_x && adjust_y==b.adjust_y && route_geometry==b.route_geometry && pixel_geometry==b.pixel_geometry && windowed==b.windowed && ibm_vga==b.ibm_vga && unlocked_never==b.unlocked_never &&
            keyboard==b.keyboard && mouse==b.mouse && sensitivity==b.sensitivity && notify_off==b.notify_off &&
            viewport.x==b.viewport.x && viewport.y==b.viewport.y && viewport.width==b.viewport.width &&
            viewport.height==b.viewport.height && user_width==b.user_width && user_height==b.user_height;
    }
};
class ProfileClock {
    Profile last_;
    uint64_t generation_=0;
    bool initialized_=false,exhausted_=false;
public:
    bool observe(const Profile &profile) {
        if(exhausted_)return false;
        if(!initialized_ || !profile.same(last_)) {
            if(generation_==UINT64_MAX){exhausted_=true;return false;}
            ++generation_;last_=profile;initialized_=true;
        }
        return true;
    }
    uint64_t generation() const {return generation_;}
};
}
// Real emulator-thread owning route checks (not guest consumption receipts).
bool KEYBOARD_NativeInputProfile();
uint64_t KEYBOARD_NativeInputTuple();
bool Mouse_NativeInputProfile();
void Mouse_NativeProfileTuple(NativeInput::Profile &profile);
NativeInput::Profile GFX_NativeInputProfile();
#endif
