PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_mul
	mov	_var_00, #0
LR__0001
	shr	arg2, #1 wc,wz
 if_c	add	_var_00, arg1
	shl	arg1, #1
 if_ne	jmp	#LR__0001
	mov	result1, _var_00
_mul_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_00
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
