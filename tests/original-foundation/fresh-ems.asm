; Explicit continuation consumes reconstructed EMS INT67 service after a fresh
; held install. Query version and page inventory; root99 denotes failed checks.
bits 16
org 100h
    nop
    mov ah,46h
    int 67h
    test ah,ah
    jnz failed
    cmp al,40h
    jb failed
    mov ah,42h
    int 67h
    test ah,ah
    jnz failed
    test dx,dx
    jz failed
    cmp bx,dx
    ja failed
    mov ah,43h
    mov bx,1
    int 67h
    test ah,ah
    jnz failed
    mov [handle],dx
    mov ax,4400h
    xor bx,bx
    int 67h
    test ah,ah
    jnz failed
    mov ah,41h
    int 67h
    test ah,ah
    jnz failed
    mov es,bx
    mov word [es:0],0a55ah
    cmp word [es:0],0a55ah
    jne failed
    mov dx,[handle]
    mov ah,45h
    int 67h
    test ah,ah
    jnz failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
handle dw 0
