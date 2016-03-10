DAT
	org	0

_test
L_025_
	mov	_test_x, INA
	sar	_test_x, #1
	and	_test_x, #1 wz
 if_ne	jmp	#L_026_
	mov	test_tmp001_, INA
	sar	test_tmp001_, #2
	test	test_tmp001_, #1 wz
 if_ne	jmp	#L_025_
	xor	OUTA, #1
	jmp	#L_025_
L_026_
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
