DAT
	org	0

_strlen2
	mov	_strlen2_r, #0
L_047_
	rdbyte	strlen2_tmp001_, arg1 wz
 if_ne	add	arg1, #1
 if_ne	add	_strlen2_r, #1
 if_ne	jmp	#L_047_
	mov	result1, _strlen2_r
_strlen2_ret
	ret

_strlen2_r
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
strlen2_tmp001_
	long	0
	fit	496
