PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fibo
	wrlong	_fibo_x, sp
	add	sp, #4
	wrlong	fibo_tmp003_, sp
	add	sp, #4
	wrlong	_fibo_ret, sp
	add	sp, #4
	mov	_fibo_x, arg1
	cmps	_fibo_x, #2 wc,wz
 if_b	mov	result1, _fibo_x
 if_b	jmp	#LR__0001
	mov	arg1, _fibo_x
	sub	arg1, #1
	sub	_fibo_x, #2
	call	#_fibo
	mov	fibo_tmp003_, result1
	mov	arg1, _fibo_x
	call	#_fibo
	add	fibo_tmp003_, result1
	mov	result1, fibo_tmp003_
LR__0001
	sub	sp, #4
	rdlong	_fibo_ret, sp
	sub	sp, #4
	rdlong	fibo_tmp003_, sp
	sub	sp, #4
	rdlong	_fibo_x, sp
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
_fibo_x
	res	1
arg1
	res	1
fibo_tmp003_
	res	1
	fit	496
