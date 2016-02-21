DAT
	org	0
test
L_001_
	mov	test_x_, INA
	sar	test_x_, #1
	and	test_x_, #1
	cmps	test_x_, #0 wz
 if_ne	jmp	#L_002_
	mov	test_tmp002_, INA
	sar	test_tmp002_, #2
	and	test_tmp002_, #1
	cmps	test_tmp002_, #0 wz
 if_ne	jmp	#L_001_
	xor	OUTA, #1
	jmp	#L_001_
L_002_
test_ret
	ret

test_tmp002_
	long	0
test_x_
	long	0
