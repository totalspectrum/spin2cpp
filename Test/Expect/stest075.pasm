PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_mul
	mov	_var_r1, #0
L__0002
	shr	arg2, #1 wc,wz
 if_c	add	_var_r1, arg1
	shl	arg1, #1
 if_ne	jmp	#L__0002
	mov	result1, _var_r1
_mul_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_r1
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
