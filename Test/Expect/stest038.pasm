DAT
	org	0

test
L_001_
	mov	test_x_, INA
	sar	test_x_, #1
	and	test_x_, #1 wz
 if_ne	jmp	#L_002_
	mov	test_tmp001_, INA
	sar	test_tmp001_, #2
	test	test_tmp001_, #1 wz
 if_ne	jmp	#L_001_
	xor	OUTA, #1
	jmp	#L_001_
L_002_
test_ret
	ret

arg1_
	long	0
test_tmp001_
	long	0
test_x_
	long	0
