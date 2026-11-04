; Exercise represented BIOS keyboard ring state and native INT16 consumer.
; Captured at NOP; explicit continuation alone inserts/consumes a key.
bits 16
org 100h
    nop
    xor ax,ax
    mov es,ax
    mov word [es:041ah],001eh
    mov word [es:041ch],0020h
    mov word [es:041eh],1e61h
    mov ah,1
    int 16h
    jz failed
    cmp ax,1e61h
    jne failed
    xor ah,ah
    int 16h
    cmp ax,1e61h
    jne failed
    cmp word [es:041ah],0020h
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
