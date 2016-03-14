DAT
	org	0

_ismagic
	cmps	arg1, #511 wz
 if_e	mov	_ismagic_a, #1
 if_ne	mov	_ismagic_a, #0
	mov	result1, _ismagic_a
_ismagic_ret
	ret

_ismagic_a
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
	fit	496
