; Pending 8042 D1 and unread p60 survive capture without port replay.
bits 16
org 100h
start:
    cli
    mov al,0d1h
    out 64h,al
    mov al,3
    out 60h,al
    mov al,0d0h
    out 64h,al
    mov al,0d1h
    out 64h,al
    jmp held
    times 100h-($-start) db 90h
held:
    nop
    in al,60h
    cmp al,3
    jne failed
    mov al,1
    out 60h,al
    mov al,0d0h
    out 64h,al
    in al,60h
    cmp al,1
    jne failed
    mov al,0d1h
    out 64h,al
    mov al,3
    out 60h,al
    mov al,0d0h
    out 64h,al
    in al,60h
    cmp al,3
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
