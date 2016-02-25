DAT
	org	0
blah
	mov	blah_tmp001_, #0
	cmps	blah_x_, #0 wc,wz
 if_ge	jmp	#L_001_
	cmps	blah_y_, #0 wc,wz
 if_lt	not	blah_tmp001_, blah_tmp001_
L_001_
	mov	result_, blah_tmp001_
blah_ret
	ret

blah_tmp001_
	long	0
blah_x_
	long	0
blah_y_
	long	0
result_
	long	0
