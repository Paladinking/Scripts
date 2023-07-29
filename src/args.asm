bits 64
default rel

section .text

extern strlen
extern GetCommandLineA

global parse_args

; Copy a length r8 argument from rcx to rdx.
; Removes quotes and null-terminates.
; After, rdx point to null-terminator of string.
copy_arg:
	cmp r8, 0
	je copy_arg_exit
	cmp BYTE [rcx], 0x22
	je copy_arg_next
	mov r9b, BYTE [rcx]
	mov BYTE [rdx], r9b
	inc rdx
copy_arg_next:
	dec r8
	inc rcx
	jmp copy_arg
copy_arg_exit:
	mov BYTE [rdx], 0
	ret


; rcx = pointer to buffer, rdx = size of buffer
; rax => number of string 
parse_args:
	sub rsp, 40
	mov QWORD [rsp + 32], rcx
	mov QWORD [rsp + 24], rdx
	call GetCommandLineA
	mov QWORD [rsp + 16], rax
	mov QWORD [rsp + 8], 0
	mov rcx, rax
	call strlen
	cmp rax, QWORD [rsp + 24]
	ja parse_args_error
	mov r8, QWORD [rsp + 16]
	mov rdx, QWORD [rsp + 32]
parse_args_loop:
	cmp BYTE [r8], 0x22
	je parse_args_loop_quote
	cmp BYTE [r8], 0x20
	je parse_args_loop_end
	cmp BYTE [r8], 0
	je parse_args_loop_end
	inc r8
	jmp parse_args_loop
parse_args_loop_quote:
	inc r8
	cmp BYTE [r8], 0
	je parse_args_loop_end
	cmp BYTE [r8], 0x22
	jne parse_args_loop_quote
	inc r8
	jmp parse_args_loop
parse_args_loop_end:
	; At this point: r8 -> BYTE after word
	mov rcx, QWORD [rsp + 16]
	mov rax, r8
	sub r8, rcx
	inc QWORD [rsp + 8]
	call copy_arg
	inc rdx
	dec rax
parse_args_skip_spaces:
	inc rax
	cmp BYTE [rax], 0
	je parse_args_end
	cmp BYTE [rax], 0x20
	je parse_args_skip_spaces
	mov r8, rax
	mov QWORD [rsp + 16], rax
	jmp parse_args_loop
parse_args_end:
	mov rax, QWORD [rsp + 8]
	jmp parse_args_exit
parse_args_error:
	mov rax, 0
parse_args_exit:
	add rsp, 40
	ret