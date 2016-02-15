stest013_sum
	mov	result_, stest013_sum_x_
	add	result_, stest013_sum_y_
stest013_sum_ret
	ret

stest013_triple
	mov	stest013_triple_tmp001_, stest013_triple_x_
	mov	stest013_sum_x_, stest013_triple_x_
	mov	stest013_sum_y_, stest013_triple_x_
	call	#stest013_sum
	mov	stest013_sum_y_, result_
	mov	stest013_sum_x_, stest013_triple_tmp001_
	call	#stest013_sum
stest013_triple_ret
	ret

result_
	long	0
stest013_sum_x_
	long	0
stest013_sum_y_
	long	0
stest013_triple_tmp001_
	long	0
stest013_triple_x_
	long	0
