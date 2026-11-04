; Restored root executes a new child, receives its status, then exits42.
; Child termination must not terminate the immutable restored invocation.
bits 16
org 100h
    nop
    mov sp,stack_top
    mov bx,100h
    mov ah,4ah
    int 21h
    jc failed
    mov ax,cs
    mov [tail_segment],ax
    mov [fcb1_segment],ax
    mov [fcb2_segment],ax
    mov dx,child
    mov bx,parameters
    mov ax,4b00h
    int 21h
    jc failed
    mov ah,4dh
    int 21h
%ifdef CHILD_TSR
    cmp ax,0307h ; AH=3 resident child exit type, AL=7
%else
    cmp ax,7
%endif
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
child db 'CHILD.COM',0
parameters:
    dw 0
    dw 80h
tail_segment dw 0
    dw 5ch
fcb1_segment dw 0
    dw 6ch
fcb2_segment dw 0
; Remain inside the retained 100h paragraphs after AH4A shrinks our MCB.
times 512 db 0
stack_top:
