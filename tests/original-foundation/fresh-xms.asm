; Root saved before XMS discovery; explicit continuation calls the reconstructed
; hookable native XMS service and verifies its version before root42 termination.
bits 16
org 100h
    nop
    mov ax,4300h
    int 2fh
    cmp al,80h
    jne failed
    mov ax,4310h
    int 2fh
    mov [entry],bx
    mov [entry+2],es
    xor ah,ah
    call far [entry]
    cmp ax,0200h
    jb failed
    ; Native allocation/handle release, not version discovery alone.
    mov ah,9
    mov dx,16
    call far [entry]
    cmp ax,1
    jne failed
    mov [handle],dx
    mov ah,0ch
    call far [entry]
    cmp ax,1
    jne failed
    mov dx,[handle]
    mov ah,0dh
    call far [entry]
    cmp ax,1
    jne failed
    mov dx,[handle]
    mov ah,0ah
    call far [entry]
    cmp ax,1
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
entry dd 0
handle dw 0
