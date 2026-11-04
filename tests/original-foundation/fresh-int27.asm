; INT27 keeps the root resident after its ordinary resize prefix. The restored
; DOS_Terminate entry cut must prevent returning to the producer's native stack.
bits 16
org 100h
    nop
    mov dx,200h
    int 27h
    jmp $
