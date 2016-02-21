DAT
	org	0
sum2
	add	sum2_x_, #8
	rdlong	result_, sum2_x_
	add	sum2_x_, #4
	rdlong	sum2_tmp003_, sum2_x_
	add	result_, sum2_tmp003_
sum2_ret
	ret

result_
	long	0
sum2_tmp003_
	long	0
sum2_x_
	long	0
