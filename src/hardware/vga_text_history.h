// Original VGA scratch producer history. No independent record/installer.
struct TextProducerHistory {
    uint8_t valid=0,uniform=0,nibble=0,period=1;
    uint32_t width=0,pixel=0;
    uint8_t shifts[4]={16,8,0,24};
};
static TextProducerHistory text_history;
static void TextHistoryForget(){text_history.valid=text_history.uniform=0;}
static unsigned TextHistoryByte(uint32_t pixel,unsigned byte){
    const unsigned local[4]={GFX_Rshift,GFX_Gshift,GFX_Bshift,24};
    for(unsigned c=0;c<4;++c)
        if(text_history.shifts[c]==byte*8u)return (pixel>>local[c])&255u;
    E_Exit("Invalid VGA text producer channels");return 0;
}
static void TextHistoryObserve(){
    const size_t at=16u+(vga.draw.width&~1u);
    if(!TempLine || at+4>TempLineSize)E_Exit("VGA text history exceeds storage");
    uint32_t pixel=0;memcpy(&pixel,TempLine+(at&~size_t(3)),4);
    text_history.nibble=TextHistoryByte(pixel,unsigned(at&3))&15u;
}
static void TextHistoryBegin(){
    if(!text_history.valid || text_history.width!=vga.draw.width){
        text_history.shifts[0]=GFX_Rshift;text_history.shifts[1]=GFX_Gshift;
        text_history.shifts[2]=GFX_Bshift;text_history.shifts[3]=24;
        if(text_history.uniform){
            uint8_t pattern[4]={};
            if(text_history.period==1)pattern[0]=uint8_t(text_history.pixel);
            else if(text_history.period==2){const uint16_t word=uint16_t(text_history.pixel);memcpy(pattern,&word,2);}
            else memcpy(pattern,&text_history.pixel,4);
            text_history.nibble=pattern[(16u+(vga.draw.width&~1u))%text_history.period]&15;
        }else TextHistoryObserve();
        text_history.width=vga.draw.width;
    }
    const size_t at=16u+(vga.draw.width&~1u);
    if(!TempLine || at>=TempLineSize)E_Exit("VGA text feedback exceeds storage");
    TempLine[at]=(TempLine[at]&0xf0)|text_history.nibble;
    text_history.valid=1;text_history.uniform=0;
}
static void TextHistoryUniform(uint32_t pixel,unsigned bytes){
    text_history.valid=0;text_history.uniform=1;
    text_history.pixel=pixel;text_history.period=uint8_t(bytes);
}
// Apply the actual writer's source-byte arithmetic. Do not reinterpret the
// overlay's intentionally non-RGBA blue addition as a destination RGB value.
static void TextHistoryXor(uint8_t *output,bool flip,bool polled,unsigned line){
    if(!flip && !polled)return;
    text_history.uniform=0;
    if(!text_history.valid || text_history.width!=vga.draw.width || vga.draw.bpp!=32){text_history.valid=0;return;}
    const uintptr_t base=reinterpret_cast<uintptr_t>(TempLine),address=reinterpret_cast<uintptr_t>(output);
    if(address<base || address-base>TempLineSize){text_history.valid=0;return;}
    const size_t coordinate=16u+(vga.draw.width&~1u),offset=address-base;
    if(coordinate>=offset){
        const size_t relative=coordinate-offset;
        if(flip && relative<size_t(vga.draw.width)*4)text_history.nibble^=15;
        if(polled && relative/4<size_t(vga.draw.width>>1)*2 && (relative&3u)==1 && ((relative/4)&1u)==(line&1u))text_history.nibble^=15;
    }
}
static void TextHistoryOverlay(const uint8_t *rgb,size_t offset){
    if(!text_history.valid)return;
    const size_t at=16u+(vga.draw.width&~1u);
    if(at>=offset && at-offset<4){
        const uint32_t source=(uint32_t(rgb[0])<<text_history.shifts[0])+
            (uint32_t(rgb[1])<<text_history.shifts[1])+(rgb[2]+text_history.shifts[2]);
        text_history.nibble=(source>>((at-offset)*8))&15;
    }
}
static bool TextHistoryValid(){
    unsigned used=0;
    for(unsigned shift:text_history.shifts){
        if(shift>24 || shift%8 || (used&(1u<<(shift/8))))return false;
        used|=1u<<(shift/8);
    }
    return text_history.valid<=1 && text_history.uniform<=1 &&
        !(text_history.valid && text_history.uniform) && text_history.nibble<16 &&
        (!text_history.valid || (text_history.width && text_history.width<=65536)) &&
        (text_history.period==1 || text_history.period==2 || text_history.period==4);
}
