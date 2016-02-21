DAT
	org	0
sum
	add	sum_x_, sum_y_
	mov	result_, sum_x_
sum_ret
	ret

inc
	mov	sum_x_, inc_x_
	mov	sum_y_, #1
	call	#sum
inc_ret
	ret

inc_x_
	long	0
result_
	long	0
sum_x_
	long	0
sum_y_
	long	0
