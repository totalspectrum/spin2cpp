DAT
	org	0

_blah
	mov	result1, #3
_blah_ret
	ret

_bar
	add	arg1, #1
	mov	result1, arg1
_bar_ret
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
