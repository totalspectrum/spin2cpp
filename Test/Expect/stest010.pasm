DAT
	org	0

_test1
	add	arg1, arg1
	add	arg2, arg2
	xor	arg1, arg2
	mov	result1, arg1
_test1_ret
	ret

_test2
	add	arg1, arg1
	add	arg2, arg2
	xor	arg1, arg2
	mov	result1, arg1
_test2_ret
	ret

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
