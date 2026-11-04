; Exercise represented BIOS keyboard ring state and native INT16 consumer.
; Captured at NOP; explicit continuation alone inserts/consumes a key.
bits 16
org 100h
program_start:
    nop
    xor ax,ax
    mov es,ax
    mov word [es:041ah],001eh
    mov word [es:041ch],0020h
    mov word [es:041eh],1e61h
%ifdef CAPTURE_ACTIVE
    jmp active_capture
    times 100h-($-program_start) db 90h
active_capture:
    nop ; IP0200: key exists in saved ring, never reinserted after import
%endif
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
