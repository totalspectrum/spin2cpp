DAT
	org	0
sum
	add	arg1_, arg2_
	mov	result_, arg1_
sum_ret
	ret

inc
	add	arg1_, #1
	mov	result_, arg1_
inc_ret
	ret

one
	mov	result_, #1
one_ret
	ret

arg1_
	long	0
arg2_
	long	0
result_
	long	0
