; UART loopback queues and register history, held before consumption.
bits 16
org 100h
start:
    nop
    mov dx,3fbh
    mov al,80h
    out dx,al
    mov dx,3f8h
    mov al,12
    out dx,al
    inc dx
    xor al,al
    out dx,al
    mov dx,3fbh
    mov al,3
    out dx,al
    mov dx,3fah
    mov al,1
    out dx,al
    mov dx,3fch
    mov al,1fh
    out dx,al
    mov dx,3ffh
    mov al,0a7h
    out dx,al
    mov dx,3f8h
    mov al,55h
    out dx,al
    mov cx,0ffffh
wait_rx:
    mov dx,3fdh
    in al,dx
    test al,1
    jnz ready
    loop wait_rx
    jmp failed
ready:
    ; Leave a second byte in flight: its PIC callback and UART state must join.
    mov dx,3f8h
    mov al,66h
    out dx,al
    jmp capture
    times 100h-($-start) db 90h
capture:
    nop
    mov dx,3ffh
    in al,dx
    cmp al,0a7h
    jne failed
    mov dx,3fbh
    in al,dx
    cmp al,3
    jne failed
    mov dx,3f8h
    in al,dx
    cmp al,55h
    jne failed
    mov cx,0ffffh
wait_second:
    mov dx,3fdh
    in al,dx
    test al,1
    jnz second
    loop wait_second
    jmp failed
second:
    mov dx,3f8h
    in al,dx
    cmp al,66h
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
