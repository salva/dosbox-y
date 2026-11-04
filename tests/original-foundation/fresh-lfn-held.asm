; Real LFN native search left open at a stopped capture frontier.
bits 16
org 100h
    nop
    push ds
    pop es
    mov dx,pattern
    mov di,result
    xor cx,cx
    xor si,si
    mov ax,714eh
    int 21h
    jc failed
    mov [handle],ax
    jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint:
    nop
    mov bx,[handle]
    mov ax,71a1h
    int 21h
    jc failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
pattern db '*.DAT',0
handle dw 0
result times 320 db 0
