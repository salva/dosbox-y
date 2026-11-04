; Native INT33 service after explicit continuation; not physical input/raster parity.
bits 16
org 100h
program_start:
    nop
    xor ax,ax
    int 33h
    cmp ax,0ffffh
    jne failed
    mov ax,4
    mov cx,160
    mov dx,80
    int 33h
%ifdef CAPTURE_ACTIVE
    jmp active_capture
    times 100h-($-program_start) db 90h
active_capture:
    nop ; IP0200: consume saved cursor position, do not reset/reinsert it
%endif
    mov ax,3
    int 33h
    cmp cx,160
    jne failed
    cmp dx,80
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
