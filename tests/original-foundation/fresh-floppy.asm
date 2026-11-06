; BIOS-mounted floppy: saved DMA buffer and current medium reads after restore.
bits 16
org 100h
%ifndef SECTORS_PER_TRACK
%define SECTORS_PER_TRACK 18
%endif
start:
    nop
    push cs
    pop es
    call read_sector
    cmp word [buffer+510],0aa55h
    jne failed
    call geometry
    jmp capture
    times 100h-($-start) db 90h
capture:
    nop
    call geometry
    cmp word [buffer+510],0aa55h
    jne failed
    mov word [buffer+510],0
    call read_sector
    cmp word [buffer+510],0aa55h
    jne failed
    mov ax,4c2ah
    int 21h
geometry:
    ; Preserve the DMA segment while AH08 returns the native parameter table.
    push es
    xor dx,dx
    mov ah,08h
    int 13h
    pop es
    jc failed
    cmp ch,79
    jne failed
    and cl,3fh
    cmp cl,SECTORS_PER_TRACK
    jne failed
    cmp dh,1
    jne failed
    ret
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
