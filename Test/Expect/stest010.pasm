DAT
	org	0
test1
	add	test1_x_, test1_x_
	add	test1_y_, test1_y_
	xor	test1_x_, test1_y_
	mov	result_, test1_x_
test1_ret
	ret

test2
	add	test2_x_, test2_x_
	add	test2_y_, test2_y_
	xor	test2_x_, test2_y_
	mov	result_, test2_x_
test2_ret
	ret

result_
	long	0
test1_x_
	long	0
test1_y_
	long	0
test2_x_
	long	0
test2_y_
	long	0
