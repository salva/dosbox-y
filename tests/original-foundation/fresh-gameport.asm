; Active logical centered gameport: no physical host input or timing replay.
bits 16
org 100h
start:
    cli
    mov dx,201h
    out dx,al
%ifndef TIMED
    mov cx,7
prefix:
    in al,dx
    loop prefix
%endif
    jmp held
    times 100h-($-start) db 90h
held:
    nop
%ifdef TIMED
    in al,dx
    and al,3
    cmp al,3
    jne failed
    mov cx,0ffffh
wait_discharge:
    in al,dx
    and al,3
    jz passed
    loop wait_discharge
    jmp failed
%else
    mov cx,57
remaining:
    in al,dx
    and al,3
    cmp al,3
    jne failed
    loop remaining
    in al,dx
    test al,3
    jnz failed
%endif
passed:
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
