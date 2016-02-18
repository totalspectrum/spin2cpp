DAT
	org	0
stest026_test1
	mov	stest026_test1_tmp001_, #0
	cmp	stest026_test1_x_, stest026_test1_y_ wc,wz
 if_lt	not	stest026_test1_tmp001_, stest026_test1_tmp001_
	mov	result_, stest026_test1_tmp001_
stest026_test1_ret
	ret

result_
	long	0
stest026_test1_tmp001_
	long	0
stest026_test1_x_
	long	0
stest026_test1_y_
	long	0
