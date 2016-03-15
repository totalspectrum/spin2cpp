CON
	incval = 15
DAT
	org	0

_inc
	add	arg1, #15
	mov	result1, arg1
_inc_ret
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
