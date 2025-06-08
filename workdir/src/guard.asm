format MS64 COFF

include "win64w.inc"

public guard_init
public guard_shutdown
public guard_start
public guard_trigger
public guard_end

define OWORD DQWORD
define resq rq
define resd rd
define resw rw
define resb rb

extrn TlsAlloc
extrn TlsFree
extrn TlsGetValue
extrn TlsSetValue

extrn GetProcessHeap
extrn HeapAlloc
extrn HeapFree

section ".bss" readable writable
TLS_INDEX: rd 1

section ".text" code readable executable


; unsigned guard_init()
guard_init:
    sub rsp, 40
    mov eax, 1
    cmp DWORD [TLS_INDEX], 0
    je .alloc
    cmp DWORD [TLS_INDEX], 0xFFFFFFFF
    jne .exit
 .alloc:
    call TlsAlloc
    mov DWORD [TLS_INDEX], eax
    inc eax
 .exit:
    add rsp, 40
    ret

guard_shutdown:
    sub rsp, 40
    mov ecx, DWORD [TLS_INDEX]
    call TlsFree
    xor rax, rax
    add rsp, 40
    ret


guard_end:
    sub rsp, 32
    push rbx
    call GetProcessHeap
    mov rbx, rax
    mov ecx, DWORD [TLS_INDEX]
    call TlsGetValue
    mov rcx, rbx
    xor rdx, rdx
    mov r8, rax
    call HeapFree
 .exit:
    pop rbx
    add rsp, 32
    ret

guard_start:
    sub rsp, 40
    call GetProcessHeap
    mov rcx, rax
    xor rdx, rdx
    mov r8, 240
    call HeapAlloc
    test rax, rax
    je .fail
    mov QWORD [rax], r12
    mov QWORD [rax + 8], r13
    mov QWORD [rax + 16], r14
    mov QWORD [rax + 24], r15
    mov QWORD [rax + 32], rdi
    mov QWORD [rax + 40], rsi
    mov QWORD [rax + 48], rbx
    mov QWORD [rax + 56], rbp
    lea rcx, [rsp + 48] ; stack before call
    mov QWORD [rax + 64], rcx
    movups DQWORD [rax + 72], XMM6
    movups DQWORD [rax + 88], XMM7
    movups DQWORD [rax + 104], XMM8
    movups DQWORD [rax + 120], XMM9
    movups DQWORD [rax + 136], XMM10
    movups DQWORD [rax + 152], XMM11
    movups DQWORD [rax + 168], XMM12
    movups DQWORD [rax + 184], XMM13
    movups DQWORD [rax + 200], XMM14
    movups DQWORD [rax + 216], XMM15
    mov rcx, QWORD [rsp + 40] ; return address
    mov QWORD [rax + 232], rcx

    mov ecx, DWORD [TLS_INDEX]
    mov rdx, rax
    call TlsSetValue
    mov rax, 1
    jmp .exit
 .fail:
    mov ecx, DWORD [TLS_INDEX]
    xor rdx, rdx
    call TlsSetValue
    xor rax, rax
 .exit:
    add rsp, 40
    ret

guard_trigger:
    add rsp, 40
    mov ecx, DWORD [TLS_INDEX]
    call TlsGetValue
    mov r12, QWORD [rax]
    mov r13, QWORD [rax + 8]
    mov r14, QWORD [rax + 16]
    mov r15, QWORD [rax + 24]
    mov rdi, QWORD [rax + 32]
    mov rsi, QWORD [rax + 40]
    mov rbx, QWORD [rax + 48]
    mov rbp, QWORD [rax + 56]
    mov rsp, QWORD [rax + 64]
    movups XMM6, DQWORD [rax + 72]
    movups XMM7, DQWORD [rax + 88]
    movups XMM8, DQWORD [rax + 104]
    movups XMM9, DQWORD [rax + 120]
    movups XMM10, DQWORD [rax + 136]
    movups XMM11, DQWORD [rax + 152]
    movups XMM12, DQWORD [rax + 168]
    movups XMM13, DQWORD [rax + 184]
    movups XMM14, DQWORD [rax + 200]
    movups XMM15, DQWORD [rax + 216]
    mov rcx, rax
    xor rax, rax
    jmp QWORD [rcx + 232]
