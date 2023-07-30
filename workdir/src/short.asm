bits 64
default rel

section .rdata
text: db "Hello world", 0xA

section .bss
argv: resb 10 
argc: resd 1

path: resb 261

section .text


extern GetStdHandle
extern WriteConsoleA
extern GetShortPathNameA
extern GetLastError
extern GetFullPathNameA

extern memcopy
extern strlen
extern parse_args

global main



main:
	sub rsp, 40
	lea rcx, [argv]
	mov rdx, 1024
	call parse_args
	mov DWORD [argc], eax
	lea rcx, [argv]
	call strlen
	cmp rax, 260
	jae main_exit_good
	lea rcx, [argv]
	lea rdx, [path]
	mov r8, rax
	call memcopy

	lea rcx, [path]
	lea r8, [path]
	mov rdx, 261
	mov r9, 0
	call GetFullPathNameA
	cmp rax, 0
	jne main_shorten
	call GetLastError
	jmp main_exit
main_shorten:
	lea rcx, [path]
	lea rdx, [path]
	mov r8, 261
	call GetShortPathNameA 
	cmp rax, 0
	jne main_output
	call GetLastError
	jmp main_exit
main_output:	
	mov DWORD [argc], eax
	mov rcx, 0xfffffff5
	call GetStdHandle
	mov rcx, rax
	lea rdx, [path]
	mov r8d, DWORD [argc] 
	mov r9, 0
	mov QWORD [rsp + 8], 0
	call WriteConsoleA
main_exit_good:
	xor rax, rax
main_exit:
	add rsp, 40
	ret
	