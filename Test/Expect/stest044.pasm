DAT
	org	0

_strlen
	neg	_strlen_r, #1
L_032_
	rdbyte	_strlen_c, arg1 wz
	add	arg1, #1
	add	_strlen_r, #1
 if_ne	jmp	#L_032_
	mov	result1, _strlen_r
_strlen_ret
	ret

_strlen_c
	long	0
_strlen_r
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
