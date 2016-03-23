DAT
	org	0

_test
L__0001
	mov	_test_x, INA
	sar	_test_x, #1
	and	_test_x, #1 wz
 if_ne	jmp	#L__0002
	mov	test_tmp001_, INA
	sar	test_tmp001_, #2
	test	test_tmp001_, #1 wz
 if_ne	jmp	#L__0001
	xor	OUTA, #1
	jmp	#L__0001
L__0002
_test_ret
	ret

_test_x
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
test_tmp001_
	long	0
	fit	496
