; SB16 DSP reset response pending at capture; consume without resetting again.
; Exercises saved FIFO and actual native port command dispatch, not waveform.
bits 16
org 100h
program_start:
    nop
    mov dx,226h
    mov al,1
    out dx,al
    mov cx,100
pulse:
    loop pulse
    xor al,al
    out dx,al
    mov dx,22eh
    mov cx,0ffffh
ready:
    in al,dx
    test al,80h
    jnz armed
    loop ready
    jmp fail
armed:
    jmp capture
    times 100h-($-program_start) db 90h
capture: ; IP0200, DSP response unread
    nop
    mov dx,22eh
    in al,dx
    test al,80h
    jz fail
    mov dx,22ah
    in al,dx
    cmp al,0aah
    jne fail
    mov dx,22ch
    mov cx,0ffffh
write_ready:
    in al,dx
    test al,80h
    jz version
    loop write_ready
    jmp fail
version:
    mov al,0e1h
    out dx,al
    mov dx,22eh
    mov cx,0ffffh
version_ready:
    in al,dx
    test al,80h
    jnz read_version
    loop version_ready
    jmp fail
read_version:
    mov dx,22ah
    in al,dx
    cmp al,4
    jb fail
    in al,dx
    mov ax,4c2ah
    int 21h
fail:
    mov ax,4c63h
    int 21h
