; Real BIOS IRQ1 Pause, captured inside a guest IRQ0 used to release it.
; All interrupt frames are guest state; continuation clears latch once then IRET.
bits 16
org 100h
start:
    nop
    cli
    xor ax,ax
    mov es,ax
    mov ax,[es:20h]
    mov [old_timer],ax
    mov ax,[es:22h]
    mov [old_timer+2],ax
    mov word [es:20h],timer
    mov [es:22h],cs
    or byte [es:496h],1         ; E1 prefix for Pause 45h
    mov al,0d2h
    out 64h,al
    mov al,45h
    out 60h,al
ready:
    in al,64h
    test al,1
    jz ready
    mov bp,sp
    mov bx,0a55ah
    int 9                     ; actual native BIOS owner enters guest wait
    cmp sp,bp
    jne failed
    cmp bx,0a55ah
    jne failed
    test byte [es:418h],8
    jnz failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
old_timer: dd 0
    times 100h-($-start) db 90h
timer:
    nop                       ; IP0200, Pause latch and wait frame active
    and byte [es:418h],0f7h
    jmp far [cs:old_timer]
