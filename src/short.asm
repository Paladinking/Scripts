bits 64
default rel

section .rdata
text: db "Hello world", 0xA

section .bss
argv: resq 1
argc: resd 1

path: resb 261

section .text


extern GetStdHandle
extern WriteConsoleA
extern GetShortPathNameA
extern GetCommandLineA
extern GetLastError
extern GetFullPathNameA

extern memcopy

global main

; rcx = f(rcx, rdx) BYTE* find(BYTE* string, BYTE c)
; Return a pointer to first instance of c in string, or to the end.
find:
	cmp BYTE [rcx], 0
	je find_exit
find_not_null:
	cmp BYTE [rcx], dl
	je find_exit
	inc rcx
	jmp find
find_exit:
	ret

find_not:
	cmp BYTE [rcx], 0
	je find_not_exit
	cmp BYTE [rcx], dl
	jne find_not_exit
	inc rcx
	jmp find_not
find_not_exit:
	ret

; rcx = f(rcx, rdx, r8) BYTE* find(BYTE* string, BYTE c1, BYTE c2)
; Return a pointer to first instance of c1 or c2 in string, or to the end.
find2:
	cmp BYTE [rcx], 0
	je find2_exit
	cmp BYTE [rcx], dl
	je find2_exit
	cmp BYTE [rcx], r8b
	je find2_exit
	inc rcx
	jmp find2
find2_exit:
	ret
	
; rcx = f(rcx) BYTE* find_next_word(BYTE* string)
; Return a pointer to next (Space separated) word in string.
find_next_word:
	mov rdx, 0x20 ; Space
	call find
	cmp BYTE [rcx], 0
	jne find_next_word_skip_space
	xor rcx, rcx
	jmp find_next_word_exit
find_next_word_skip_space:
	call find_not
	cmp BYTE [rcx], 0
	jne find_next_word_exit
	xor rcx, rcx
find_next_word_exit:
	ret

; rcx = f(rcx) DWORD find_word_len(BYTE* string)
; Return the length of a word in string in rcx.
find_word_len:
	mov rax, rcx
	mov rdx, 0x22
	mov r8, 0x20
find_word_len_loop:
	call find2
	cmp BYTE [rcx], 0
	je find_word_len_end
	cmp BYTE [rcx], 0x20
	je find_word_len_end
	inc rcx
	call find
	cmp BYTE [rcx], 0
	je find_word_len_end
	inc rcx
	jmp find_word_len_loop
find_word_len_end:
	sub rcx, rax
	ret



; Copy a length r8 argument from rcx to rdx.
; Removes outer quotes and null-terminates.
copy_arg:
	cmp BYTE [rcx], 0x22 ; 0x22 == '"'
	jne copy_arg_copy
	inc rcx
	dec r8
copy_arg_copy:
	call memcopy
	cmp BYTE [rdx - 1], 0x22
	jne copy_arg_exit
	dec rdx
copy_arg_exit:
	mov BYTE [rdx], 0
	ret


main:
	sub rsp, 40
	call GetCommandLineA
	mov QWORD [argv], rax
	mov rcx, rax
	call find_next_word
	cmp rcx, 0
	je main_exit_good
	mov QWORD [argv], rcx
	call find_word_len
	cmp rcx, 260
	jae main_exit_good
	mov DWORD [argc], ecx
	mov r8, rcx
	mov rcx, QWORD [argv]
	lea rdx, [path]
	call copy_arg
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
	