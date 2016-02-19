DAT
	org	0
stest036_sum2
	add	stest036_sum2_x_, #8
	rdlong	result_, stest036_sum2_x_
	add	stest036_sum2_x_, #4
	rdlong	stest036_sum2_tmp003_, stest036_sum2_x_
	add	result_, stest036_sum2_tmp003_
stest036_sum2_ret
	ret

result_
	long	0
stest036_sum2_tmp003_
	long	0
stest036_sum2_x_
	long	0
