; Native INT10 font upload, active plane2 bytes, fresh reconstruction readback.
bits 16
org 100h
start:
    nop
    mov ax,3
    int 10h
    push cs
    pop es
    mov bp,glyph
    mov ax,1100h
    mov bx,1000h ; 16 rows, font block0
    mov cx,1
    mov dx,65
    int 10h
    ; Plane2 host mapping, unchanged across capture. Preserve text contents.
    mov dx,3c4h
    mov ax,0402h
    out dx,ax
    mov ax,0704h
    out dx,ax
    mov dx,3ceh
    mov ax,0204h
    out dx,ax
    mov ax,0005h
    out dx,ax
    mov ax,0406h
    out dx,ax
    mov ax,0a000h
    mov es,ax
    jmp capture
    times 100h-($-start) db 90h
capture:
    nop ; IP0200: font contents and plane mapping must survive restore
    mov si,glyph
    mov di,65*32
    mov cx,16
verify:
    lodsb
    cmp al,[es:di]
    jne failed
    inc di
    loop verify
    ; Restore text mapping through ordinary owner ports, then BIOS service.
    mov dx,3c4h
    mov ax,0302h
    out dx,ax
    mov ax,0304h
    out dx,ax
    mov dx,3ceh
    mov ax,0004h
    out dx,ax
    mov ax,1005h
    out dx,ax
    mov ax,0e06h
    out dx,ax
    mov ax,0e41h
    mov bx,7
    int 10h
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
glyph db 0,18h,24h,42h,81h,81h,0ffh,81h,81h,81h,81h,0,55h,0aah,0,0
