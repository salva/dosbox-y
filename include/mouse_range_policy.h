/* GPL-2.0-or-later. Shared INT33 absolute screen-domain update policy. */
#ifndef DOSBOX_MOUSE_RANGE_POLICY_H
#define DOSBOX_MOUSE_RANGE_POLICY_H
namespace MouseRangePolicy {
// NEVER absolute integration has no relative drag domain to preserve. A guest
// range transition must be interpreted by the existing per-axis inference even
// with buttons held. Other emulation policies retain historical first/idle rule.
inline bool update_screen(bool first,bool buttons_held,bool unlocked_never) {
    return first || !buttons_held || unlocked_never;
}
// Standard VGA 320x200 transition only: retain an explicitly authored,
// zero-origin nondegenerate per-axis logical domain within the target defaults.
// Resets, other modes, assumption overrides and arbitrary signed subranges do
// not gain a new retention policy. Existing SVGA behavior is untouched.
inline bool retain_explicit(bool setmode,unsigned operation,unsigned mode,
                            bool explicit_range,int min,int max,int target_max,
                            int assumed_max,bool pc98) {
    return setmode && operation==4 && mode==0x13 && explicit_range &&
           !pc98 && assumed_max==0 && min==0 && max>0 && max<=target_max;
}
}
#endif
