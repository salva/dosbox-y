#ifndef DOSBOX_VGA_SCAN_ADDRESSING_H
#define DOSBOX_VGA_SCAN_ADDRESSING_H
#include "dosbox.h"
#include "vga.h"

// Effect-free addressing shared by live scan setup and state reconstruction.
inline Bitu VGA_ResolveScanAddressing(VGAModes mode, MachineType model,
    SVGACards card, bool egavga, bool integrated, Bitu integrated_stride,
    Bitu scan_len, Bitu addr_shift, Bitu blocks, Bitu address_line_total)
{
    if (integrated) return integrated_stride;
    switch (mode) {
    case M_EGA:
    case M_LIN4: return scan_len * (8u << addr_shift);
    case M_PACKED4: return scan_len * 8;
    case M_VGA:
        if (card == SVGA_TsengET3K || card == SVGA_TsengET4K)
            return scan_len * ((addr_shift == 1) ? 16 : 8);
        if (model == MCH_MCGA) return blocks * 4;
        return (unsigned int)scan_len * ((unsigned int)(2*4) << (unsigned int)addr_shift);
    case M_LIN8:
    case M_LIN15:
    case M_LIN16:
    case M_LIN24:
    case M_LIN32:
        if (card == SVGA_TsengET3K || card == SVGA_TsengET4K || card == SVGA_S3Trio)
            return scan_len * (2u << 2u);
        return scan_len * (2u << addr_shift);
    case M_TEXT:
    case M_CGA2:
    case M_CGA16:
    case M_DCGA:
    case M_AMSTRAD:
        if (egavga) return scan_len * (2u << addr_shift);
        if (model == MCH_3270PC && mode == M_CGA2)
            return blocks * address_line_total;
        return blocks;
    case M_CGA4:
        // 3270 APA scanlines are contiguous, not CGA interleaved banks.
        return model == MCH_3270PC ? blocks * address_line_total : blocks;
    case M_TANDY2: return model == MCH_MCGA ? blocks : blocks / 4u;
    case M_PC98:
    case M_TANDY4:
    case M_TANDY16:
    case M_HERC_GFX: return blocks;
    case M_TANDY_TEXT:
    case M_HERC_TEXT: return blocks * 2;
    default: return blocks * 8;
    }
}
#endif
