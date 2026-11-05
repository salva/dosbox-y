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
%ifdef CHILD_SHELL
    cmp ax,0
%else
    cmp ax,7
%endif
%endif
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
%ifdef CHILD_SHELL
child db 'Z:\COMMAND.COM',0
%else
child db 'CHILD.COM',0
%endif
parameters:
    dw 0
%ifdef CHILD_SHELL
    dw shell_tail
%else
    dw 80h
%endif
tail_segment dw 0
    dw 5ch
fcb1_segment dw 0
    dw 6ch
fcb2_segment dw 0
%ifdef CHILD_SHELL
shell_tail db 8,' /C EXIT',13
%endif
; Remain inside the retained 100h paragraphs after AH4A shrinks our MCB.
times 512 db 0
stack_top:
