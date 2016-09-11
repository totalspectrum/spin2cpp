PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_mul
	mov	_var_r1, #0
L__0001
	shr	arg2, #1 wc,wz
 if_c	add	_var_r1, arg1
	shl	arg1, #1
 if_ne	jmp	#L__0001
	mov	result1, _var_r1
_mul_ret
	ret

_var_r1
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
