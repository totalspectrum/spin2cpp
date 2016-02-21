DAT
	org	0
sum
	add	sum_x_, sum_y_
	mov	result_, sum_x_
sum_ret
	ret

result_
	long	0
sum_x_
	long	0
sum_y_
	long	0
