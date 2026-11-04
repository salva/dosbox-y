; Captured before any guest setup. Exercise native BIOS video service, guest
; text VRAM and memory after fresh reconstruction; exit99 on wrong readback.
bits 16
org 100h
program_start:
    nop
    mov ax,0003h
    int 10h
    mov ax,0b800h
    mov es,ax
    mov word [es:0],1f59h
%ifdef CAPTURE_ACTIVE
    jmp active_capture
    times 100h-($-program_start) db 90h
active_capture:
    nop ; IP0200: verify saved video mode and VRAM, not newly redrawn state
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
