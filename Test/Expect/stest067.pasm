DAT
	org	0

_fibo
	wrlong	_fibo_x, ptr__stackspace_
	add	ptr__stackspace_, #4
	wrlong	fibo_tmp004_, ptr__stackspace_
	add	ptr__stackspace_, #4
	wrlong	_fibo_ret, ptr__stackspace_
	add	ptr__stackspace_, #4
	mov	_fibo_x, arg1_
	cmps	_fibo_x, #2 wc,wz
 if_b	mov	result_, _fibo_x
 if_b	jmp	#L_032_
	mov	arg1_, _fibo_x
	sub	arg1_, #1
	call	#_fibo
	mov	fibo_tmp004_, result_
	sub	_fibo_x, #2
	mov	arg1_, _fibo_x
	call	#_fibo
	add	fibo_tmp004_, result_
	mov	result_, fibo_tmp004_
L_032_
	sub	ptr__stackspace_, #4
	rdlong	_fibo_ret, ptr__stackspace_
	sub	ptr__stackspace_, #4
	rdlong	fibo_tmp004_, ptr__stackspace_
	sub	ptr__stackspace_, #4
	rdlong	_fibo_x, ptr__stackspace_
_fibo_ret
	ret

_fibo_x
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
fibo_tmp004_
	long	0
ptr__stackspace_
	long	@@@_stackspace
result_
	long	0
	fit	496
_stackspace
	long	0
