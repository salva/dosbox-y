; Save with an allocated/locked XMS handle and meaningful extended-memory data.
; The consuming fresh process must keep the lock, read the data, then dispose
; the allocation via the reconstructed service. root99 means failed assertion.
bits 16
org 100h
    nop
    mov ax,4310h
    int 2fh
    mov [entry],bx
    mov [entry+2],es
    mov ah,9
    mov dx,16
    call far [entry]
    cmp ax,1
    jne failed
    mov [handle],dx
    mov [source_handle],dx
    mov ah,0ch
    call far [entry]
    cmp ax,1
    jne failed
    ; Return physical address DX:BX. Store a word using a temporary descriptor
    ; for XMS move from the conventional payload, not direct unreal access.
    mov ax,cs
    mov [source_segment],ax
    mov ax,[handle]
    mov [destination_handle],ax
    mov si,write_move
    mov ah,0bh
    call far [entry]
    cmp ax,1
    jne failed
    jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint: ; IP0200
    mov ax,cs
    mov [destination_segment],ax
    mov si,read_move
    mov ah,0bh
    call far [entry]
    cmp ax,1
    jne failed
    cmp word [readback],0a55ah
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
payload dw 0a55ah
readback dw 0
write_move:
 dd 2
 dw 0
 dw payload
source_segment: dw 0
destination_handle: dw 0
 dd 0
read_move:
 dd 2
source_handle: dw 0
 dd 0
 dw 0
 dw readback
destination_segment: dw 0
