DAT
	org	0
sum
	add	sum_x_, sum_y_
	mov	result_, sum_x_
sum_ret
	ret

inc
	add	inc_x_, #1
	mov	result_, inc_x_
inc_ret
	ret

one
	mov	result_, #1
one_ret
	ret

inc_x_
	long	0
result_
	long	0
sum_x_
	long	0
sum_y_
	long	0
