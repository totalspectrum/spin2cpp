DAT
	org	0

_test
	mov	DIRA, #1
	mov	OUTA, #1
_test_ret
	ret

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
