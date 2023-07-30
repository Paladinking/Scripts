bits 64
default rel

section .text

global strlen
global strcmp
global memcopy

; stdcall DWORD strlen(BYTE* string)
; Returns the length of a given null-terminated string.
; Destroys: rax, rcx
strlen:
	mov rax, rcx
strlen_loop:
	cmp BYTE [rcx], 0
	je strlen_exit
	inc rcx
	jmp strlen_loop
strlen_exit:
	sub rcx, rax
	mov rax, rcx
	ret

; Copy r8 characters from rcx to rdx
; Destroys: rcx, rdx, r8, r9
memcopy:
	cmp r8, 0
	je memcopy_exit
	mov r9b, BYTE [rcx]
	mov BYTE [rdx], r9b
	inc rcx
	inc rdx
	dec r8
	jmp memcopy
memcopy_exit:
	ret

; Compare two null-terminated strings
; Destroys: rcx, rdx, r8, r9, rax
strcmp:
	mov r8b, BYTE [rcx]
	mov r9b, BYTE [rdx]
	mov al, r8b
	sub al, r9b
	jne strcmp_end
	cmp r8b, 0
	je strcmp_end
	cmp r9b, 0
	je strcmp_end
	inc rcx
	inc rdx
	jmp strcmp
strcmp_end:
	movsx eax, al
	ret
	
	