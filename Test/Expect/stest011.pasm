DAT
	org	0

_test
	mov	DIRA, #1
	mov	OUTA, #1
_test_ret
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
