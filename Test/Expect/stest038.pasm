DAT
	org	0
stest038_test
L_001_
	mov	stest038_test_x_, INA
	sar	stest038_test_x_, #1
	and	stest038_test_x_, #1
	cmps	stest038_test_x_, #0 wz
 if_ne	jmp	#L_002_
	mov	stest038_test_tmp002_, INA
	sar	stest038_test_tmp002_, #2
	and	stest038_test_tmp002_, #1
	cmps	stest038_test_tmp002_, #0 wz
 if_ne	jmp	#L_001_
	xor	OUTA, #1
	jmp	#L_001_
L_002_
stest038_test_ret
	ret

stest038_test_tmp002_
	long	0
stest038_test_x_
	long	0
