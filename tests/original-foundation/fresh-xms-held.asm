; Save with an allocated/locked XMS handle and meaningful extended-memory data.
; The consuming fresh process must keep the lock, read the data, then dispose
; the allocation via the reconstructed service. root99 means failed assertion.
; GUEST_HOOK installs and executes a legitimate far-jump hook in the five-byte
; XMS prefix. On fresh continuation it must count all three native calls.
bits 16
org 100h
    nop
    mov ax,4310h
    int 2fh
    mov [entry],bx
    mov [entry+2],es
%ifdef GUEST_HOOK
    ; Replace only the guest-owned five-byte prefix; chain to dispatch at +5.
    mov byte [es:bx],0eah
    mov word [es:bx+1],hook
    mov ax,cs
    mov [es:bx+3],ax
%endif
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
%ifdef GUEST_HOOK
    mov word [hook_calls],0
%endif
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
%ifdef GUEST_HOOK
    cmp word [hook_calls],3 ; read, unlock, free through the restored hook
    jne failed
%endif
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
%ifdef GUEST_HOOK
hook:
    inc word [cs:hook_calls]
    push word [cs:entry+2]
    push word [cs:entry]
    push bp
    mov bp,sp
    add word [ss:bp+2],5
    pop bp
    retf
hook_calls dw 0
%endif
