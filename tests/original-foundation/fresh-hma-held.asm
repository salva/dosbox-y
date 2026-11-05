; Allocate HMA through the native DOS multiplex service before capture.
; After restore a query must retain its bump cursor and payload, not overlap it.
bits 16
org 100h
    nop
    mov ax,4a02h
    mov bx,32
    int 2fh
    cmp bx,32
    jne failed
    cmp di,0ffffh
    je failed
    mov [allocated],di
    ; HMA requires A20. Use the original XMS local-enable service.
    mov ax,4310h
    int 2fh
    mov [entry],bx
    mov [entry+2],es
    mov ah,5
    call far [entry]
    cmp ax,1
    jne failed
    mov ax,0ffffh
    mov es,ax
    mov di,[allocated]
    mov word [es:di],0a55ah
    jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint: ; IP0200
    nop
    mov ax,4a01h
    int 2fh
    mov dx,[allocated]
    add dx,32
    cmp di,dx
    jne failed
    mov di,[allocated]
    cmp word [es:di],0a55ah
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
allocated dw 0
entry dd 0
