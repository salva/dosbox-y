#ifndef DOSBOX_VGA_APPLIED_PALETTE_H
#define DOSBOX_VGA_APPLIED_PALETTE_H
#include <cstdint>
// Last applied colors, not current DAC registers. Separate banks are necessary:
// native >=24bpp writes only xlat32; low-depth writes both (with a different
// 32-bit packing rule). Unwritten entries remain explicit, not opaque host bits.
struct VGA_AppliedPalette {
    struct Color { uint8_t red=0,green=0,blue=0; uint8_t valid=0; uint8_t low_depth=0; };
    Color narrow[256]{},wide[256]{};
    // Destination packing only. No palette I/O/render calls, register changes,
    // deferred-update delivery or ticks. Both banks retain independent history.
    template<class Dac>
    void rebuild(Dac &dac,unsigned red_shift,unsigned green_shift,
                 unsigned blue_shift,uint32_t alpha) const {
        for(unsigned i=0;i<256;++i){
            const auto &n=narrow[i];
            if(n.valid)dac.xlat16[i]=uint16_t(
                (uint32_t(n.blue>>3)<<blue_shift) |
                (uint32_t(n.green>>2)<<green_shift) |
                (uint32_t(n.red>>3)<<red_shift) | alpha);
            const auto &w=wide[i];
            if(!w.valid)continue;
            if(w.low_depth){
                // Native low-depth branch also supplies a PC-98 32-bit bank.
                dac.xlat32[i]=blue_shift==0 ?
                    uint32_t(w.blue)|(uint32_t(w.green)<<8)|(uint32_t(w.red)<<16) :
                    uint32_t(w.red)|(uint32_t(w.green)<<8)|(uint32_t(w.blue)<<16);
            }else{
                dac.xlat32[i]=(uint32_t(w.blue)<<blue_shift) |
                    (uint32_t(w.green)<<green_shift) |
                    (uint32_t(w.red)<<red_shift) | alpha;
            }
        }
    }
    void apply(unsigned index,uint8_t red,uint8_t green,uint8_t blue,bool narrow_written) {
        Color color{red,green,blue,true,narrow_written};
        wide[index]=color;
        if(narrow_written)narrow[index]=color;
    }
};
// Native stopped-owner adapter; does not install renderer palette.
void VGA_DAC_RebuildAppliedLookupTables();
#endif
