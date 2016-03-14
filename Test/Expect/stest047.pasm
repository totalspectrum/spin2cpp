DAT
	org	0

_addone
	add	arg1, #1
	mov	result1, arg1
_addone_ret
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
