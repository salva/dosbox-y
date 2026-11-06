; AH86 native setup/guest wait/RTC completion, balanced IRET and flags.
bits 16
org 100h
start:
    nop
%ifdef CAPTURE_ACTIVE
    cli
    xor ax,ax
    mov es,ax
    mov ax,[es:20h]
    mov [old_timer],ax
    mov ax,[es:22h]
    mov [old_timer+2],ax
    mov word [es:20h],timer
    mov [es:22h],cs
%endif
    mov bp,sp
    stc
    mov cx,0
    mov dx,0ffffh
    mov ah,86h
    int 15h
    jc failed
    cmp sp,bp
    jne failed
    xor ax,ax
    mov es,ax
    cmp byte [es:04a0h],0       ; BIOS_WAIT_FLAG_ACTIVE
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
%ifdef CAPTURE_ACTIVE
old_timer: dd 0
    times 100h-($-start) db 90h
timer:
    nop                         ; IP0200 during AH86 guest wait
    jmp far [cs:old_timer]
%endif
