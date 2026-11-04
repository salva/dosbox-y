; Save with an allocated EMS page mapped at the native frame and nonzero data.
; Fresh consumer verifies that mapping/data, remaps the handle, and frees it.
bits 16
org 100h
    nop
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
    jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint: ; IP0200
    cmp word [es:0],0a55ah
    jne failed
    mov dx,[handle]
    mov ax,4400h
    xor bx,bx
    int 67h
    test ah,ah
    jnz failed
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
