DAT
	org	0
sum
	add	sum_x_, sum_y_
	mov	result_, sum_x_
sum_ret
	ret

triple
	mov	triple_tmp001_, triple_x_
	mov	sum_x_, triple_x_
	mov	sum_y_, triple_x_
	call	#sum
	mov	sum_y_, result_
	mov	sum_x_, triple_tmp001_
	call	#sum
triple_ret
	ret

result_
	long	0
sum_x_
	long	0
sum_y_
	long	0
triple_tmp001_
	long	0
triple_x_
	long	0
