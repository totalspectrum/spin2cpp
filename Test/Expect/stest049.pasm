DAT
	org	0

sum
	add	arg1_, arg2_
	mov	result_, arg1_
sum_ret
	ret

one
	mov	result_, #1
one_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
