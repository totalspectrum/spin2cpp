DAT
	org	0

_test
L_001_
	mov	_test_x, INA
	sar	_test_x, #1
	and	_test_x, #1 wz
 if_ne	jmp	#L_002_
	mov	test_tmp001_, INA
	sar	test_tmp001_, #2
	test	test_tmp001_, #1 wz
 if_ne	jmp	#L_001_
	xor	OUTA, #1
	jmp	#L_001_
L_002_
_test_ret
	ret

_test_x
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
test_tmp001_
	long	0
