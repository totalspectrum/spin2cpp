DAT
	org	0

_ismagic
	cmps	arg1_, #511 wz
 if_e	mov	_ismagic_a, #1
 if_ne	mov	_ismagic_a, #0
	mov	result_, _ismagic_a
_ismagic_ret
	ret

_ismagic_a
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
	fit	496
