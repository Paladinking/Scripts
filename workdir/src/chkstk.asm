bits 64
default rel

section .text

global ___chkstk_ms

___chkstk_ms:
	push rcx
	push rax
	cmp  rax, 0x1000
	lea  rcx, [rsp + 0x18]
	jb ___chkstk_ms_end
___chkstk_ms_loop:
	sub rcx, 0x1000
	or qword [rcx], 0
	sub rax, 0x1000
	cmp rax, 0x1000
	ja ___chkstk_ms_loop
___chkstk_ms_end:
	sub rcx,rax
	or  qword [rcx], 0
	pop rax
	pop rcx
	ret