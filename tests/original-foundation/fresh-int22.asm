; An installed custom PSP INT22 must not run when the restored root exits.
; The forbidden continuation would print a marker then loop forever.
bits 16
org 100h
    nop
    mov word [0ah],forbidden
    mov ax,cs
    mov [0ch],ax
    mov ax,4c2ah
    int 21h
forbidden:
    mov dx,message
    mov ah,9
    int 21h
    jmp $
message db 'UNSAFE_ROOT_INT22$',0
