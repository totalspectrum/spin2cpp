DAT
	org	0
stest035_sum2
	rdlong	result_, stest035_sum2_x_
	add	stest035_sum2_x_, #4
	rdlong	stest035_sum2_tmp003_, stest035_sum2_x_
	add	result_, stest035_sum2_tmp003_
stest035_sum2_ret
	ret

result_
	long	0
stest035_sum2_tmp003_
	long	0
stest035_sum2_x_
	long	0
