stest010_test1
	mov	result_, stest010_test1_x_
	add	result_, stest010_test1_x_
	add	stest010_test1_y_, stest010_test1_y_
	xor	result_, stest010_test1_y_
stest010_test1_ret
	ret

stest010_test2
	add	stest010_test2_x_, stest010_test2_x_
	add	stest010_test2_y_, stest010_test2_y_
	xor	stest010_test2_x_, stest010_test2_y_
	mov	result_, stest010_test2_x_
stest010_test2_ret
	ret

result_
	long	0
stest010_test1_x_
	long	0
stest010_test1_y_
	long	0
stest010_test2_x_
	long	0
stest010_test2_y_
	long	0
