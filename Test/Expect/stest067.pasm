PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fibo
	wrlong	fibo_tmp002_, sp
	add	sp, #4
	wrlong	_fibo__cse__0015, sp
	add	sp, #4
	wrlong	fibo_tmp003_, sp
	add	sp, #4
	wrlong	_fibo_ret, sp
	add	sp, #4
	cmps	arg1, #2 wc,wz
 if_b	mov	result1, arg1
 if_b	jmp	#LR__0001
	mov	fibo_tmp002_, arg1
	sub	fibo_tmp002_, #1
	sub	arg1, #2
	mov	_fibo__cse__0015, arg1
	mov	arg1, fibo_tmp002_
	call	#_fibo
	mov	fibo_tmp003_, result1
	mov	arg1, _fibo__cse__0015
	call	#_fibo
	add	fibo_tmp003_, result1
	mov	result1, fibo_tmp003_
LR__0001
	sub	sp, #4
	rdlong	_fibo_ret, sp
	sub	sp, #4
	rdlong	fibo_tmp003_, sp
	sub	sp, #4
	rdlong	_fibo__cse__0015, sp
	sub	sp, #4
	rdlong	fibo_tmp002_, sp
_fibo_ret
	ret

result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
stackspace
	long	0[1]
	org	COG_BSS_START
_fibo__cse__0015
	res	1
arg1
	res	1
fibo_tmp002_
	res	1
fibo_tmp003_
	res	1
	fit	496
