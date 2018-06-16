PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fibo
	wrlong	_local_00, sp
	add	sp, #4
	wrlong	_local_01, sp
	add	sp, #4
	wrlong	_local_02, sp
	add	sp, #4
	wrlong	_fibo_ret, sp
	add	sp, #4
	cmps	arg1, #2 wc,wz
 if_b	mov	result1, arg1
 if_b	jmp	#L__0004
	mov	_local_00, arg1
	sub	_local_00, #1
	sub	arg1, #2
	mov	_local_01, arg1
	mov	arg1, _local_00
	call	#_fibo
	mov	_local_02, result1
	mov	arg1, _local_01
	call	#_fibo
	add	_local_02, result1
	mov	result1, _local_02
L__0004
	sub	sp, #4
	rdlong	_fibo_ret, sp
	sub	sp, #4
	rdlong	_local_02, sp
	sub	sp, #4
	rdlong	_local_01, sp
	sub	sp, #4
	rdlong	_local_00, sp
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
_local_00
	res	1
_local_01
	res	1
_local_02
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
