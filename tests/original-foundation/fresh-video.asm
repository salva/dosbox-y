; Captured before any guest setup. Exercise native BIOS video service, guest
; text VRAM and memory after fresh reconstruction; exit99 on wrong readback.
bits 16
org 100h
program_start:
    nop
%ifdef VESA_SERVICE
    push cs
    pop es
    mov di,mode_info
    mov ax,4f01h
    mov cx,101h
    int 10h
    cmp ax,004fh
    jne failed
    ; Execute the returned ROM real-mode bank callback, not only INT10.
    ; Mode13-compatible bank zero is valid with the matching SVGA hardware.
    mov ax,4f02h
    mov bx,101h
    int 10h
    cmp ax,004fh
    jne failed
    mov ax,4f05h
    xor bx,bx
    xor dx,dx
    call far [mode_info+12]
    cmp ax,004fh
    jne failed
%endif
%ifdef BANK_SERVICE
    mov ax,0013h
    int 10h
    mov ax,0a000h
    mov es,ax
    ; Tseng ET4000 port 3CD: write bank in low nibble, read in high.
    mov dx,3cdh
    mov al,21h
    out dx,al
    mov byte [es:0],5ah
    mov al,11h
    out dx,al
    cmp byte [es:0],5ah
    jne failed
    mov al,22h
    out dx,al
    mov byte [es:0],0a5h
    mov al,12h
    out dx,al
    cmp byte [es:0],5ah
    jne failed
    mov byte [es:1],3ch
    mov al,22h
    out dx,al
    cmp byte [es:1],3ch
    jne failed
%ifdef CAPTURE_ACTIVE
    mov al,12h
    out dx,al
    jmp bank_capture
    times 100h-($-program_start) db 90h
bank_capture:
    nop ; IP0200: distinct read/write banks and their bytes remain active
    cmp byte [es:0],5ah
    jne failed
    mov byte [es:2],69h ; write bank2 while read bank1 is retained
    cmp byte [es:2],69h
    je failed
    mov al,22h
    out dx,al
    cmp byte [es:2],69h
    jne failed
%endif
%endif
    mov ax,0003h
    int 10h
    mov ax,0b800h
    mov es,ax
    mov word [es:0],1f59h
%ifdef CAPTURE_ACTIVE
%ifndef BANK_SERVICE
    jmp active_capture
    times 100h-($-program_start) db 90h
active_capture:
    nop ; IP0200: verify saved video mode and VRAM, not newly redrawn state
%endif
%endif
    cmp word [es:0],1f59h
    jne failed
    mov bx,0
    mov ah,2
    xor dx,dx
    int 10h
    mov ah,8
    int 10h
    cmp ax,1f59h
    jne failed
    mov word [witness],0a55ah
    cmp word [witness],0a55ah
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
witness dw 0
%ifdef VESA_SERVICE
mode_info times 256 db 0
%endif
