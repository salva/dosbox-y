bits 16
org 100h
; Capture at entry. Only explicit continuation may execute CPUID.
    nop
    xor eax,eax
    cpuid
    cmp ebx,0x756e6547
    jne failed
    cmp edx,0x49656e69
    jne failed
    cmp ecx,0x6c65746e
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
