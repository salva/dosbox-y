; BIOS-mounted floppy: saved DMA buffer and current medium reads after restore.
bits 16
org 100h
start:
    nop
    push cs
    pop es
    call read_sector
    cmp word [buffer+510],0aa55h
    jne failed
    jmp capture
    times 100h-($-start) db 90h
capture:
    nop
    cmp word [buffer+510],0aa55h
    jne failed
    mov word [buffer+510],0
    call read_sector
    cmp word [buffer+510],0aa55h
    jne failed
    mov ax,4c2ah
    int 21h
read_sector:
    mov bx,buffer
    mov ax,0201h
    mov cx,1
    xor dx,dx
    int 13h
    jc failed
    ret
failed:
    mov ax,4c63h
    int 21h
buffer times 512 db 0
