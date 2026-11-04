; Captured at NOP; explicit continuation programs actual PIT2/speaker and
; waits for two guest BIOS timer ticks before disabling sound/root42 exit.
; No host delay is substituted for guest time. An absent timer stalls/fails.
bits 16
org 100h
    nop
    mov al,0b6h
    out 43h,al
    mov ax,1193
    out 42h,al
    mov al,ah
    out 42h,al
    in al,61h
    or al,3
    out 61h,al
    xor ax,ax
    mov es,ax
    mov bx,[es:046ch]
    add bx,2
wait_tick:
    cmp [es:046ch],bx
    jb wait_tick
    in al,61h
    and al,0fch
    out 61h,al
    mov ax,4c2ah
    int 21h
