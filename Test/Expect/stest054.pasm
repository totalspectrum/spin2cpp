DAT
	org	0

_testit
	test	arg1, #4 wz
 if_ne	mov	result1, arg2
 if_e	mov	result1, #0
_testit_ret
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
