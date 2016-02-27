DAT
	org	0
test1
	mov	test1_tmp001_, #0
	cmps	test1_x_, test1_y_ wc,wz
 if_b	not	test1_tmp001_, test1_tmp001_
	mov	result_, test1_tmp001_
test1_ret
	ret

result_
	long	0
test1_tmp001_
	long	0
test1_x_
	long	0
test1_y_
	long	0
