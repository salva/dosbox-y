; Explicit fresh continuation consumes the constructor's IHSEG INT1 helper.
; Entry remains stopped at IP0100; root99 indicates altered carry/AX.
bits 16
org 100h
    nop
    mov ax,1234h
    stc
    int 1
    jnc failed
    cmp ax,1234h
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
