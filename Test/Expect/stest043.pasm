DAT
	org	0
blah
	mov	blah_tmp001_, #0
	cmps	arg1_, #0 wc,wz
 if_ae	jmp	#L_001_
	cmps	arg2_, #0 wc,wz
 if_b	not	blah_tmp001_, blah_tmp001_
L_001_
	mov	result_, blah_tmp001_
blah_ret
	ret

arg1_
	long	0
arg2_
	long	0
blah_tmp001_
	long	0
result_
	long	0
