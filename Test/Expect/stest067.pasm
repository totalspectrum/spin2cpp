PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fibo
	wrlong	fibo_tmp001_, sp
	add	sp, #4
	wrlong	_fibo__cse__0002, sp
	add	sp, #4
	wrlong	fibo_tmp002_, sp
	add	sp, #4
	wrlong	_fibo_ret, sp
	add	sp, #4
	cmps	arg1, #2 wc,wz
 if_b	mov	result1, arg1
 if_b	jmp	#L__0004
	mov	fibo_tmp001_, arg1
	sub	fibo_tmp001_, #1
	mov	_fibo__cse__0002, arg1
	sub	_fibo__cse__0002, #2
	mov	arg1, fibo_tmp001_
	call	#_fibo
	mov	fibo_tmp002_, result1
	mov	arg1, _fibo__cse__0002
	call	#_fibo
	add	fibo_tmp002_, result1
	mov	result1, fibo_tmp002_
L__0004
	sub	sp, #4
	rdlong	_fibo_ret, sp
	sub	sp, #4
	rdlong	fibo_tmp002_, sp
	sub	sp, #4
	rdlong	_fibo__cse__0002, sp
	sub	sp, #4
	rdlong	fibo_tmp001_, sp
_fibo_ret
	ret

result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
stackspace
	res	1
	org	COG_BSS_START
_fibo__cse__0002
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
fibo_tmp001_
	res	1
fibo_tmp002_
	res	1
	fit	496
