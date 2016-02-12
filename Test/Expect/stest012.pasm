stest012_sum
	mov	result_, stest012_sum_x_
	add	result_, stest012_sum_y_
stest012_sum_ret
	ret

stest012_inc
	mov	stest012_sum_x_, stest012_inc_x_
	mov	stest012_sum_y_, #1
	call	#stest012_sum
stest012_inc_ret
	ret

result_
	long	0
stest012_inc_x_
	long	0
stest012_sum_x_
	long	0
stest012_sum_y_
	long	0
