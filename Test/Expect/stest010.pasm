DAT
	org	0
test1
	add	arg1_, arg1_
	add	arg2_, arg2_
	xor	arg1_, arg2_
	mov	result_, arg1_
test1_ret
	ret

test2
	add	arg1_, arg1_
	add	arg2_, arg2_
	xor	arg1_, arg2_
	mov	result_, arg1_
test2_ret
	ret

arg1_
	long	0
arg2_
	long	0
result_
	long	0
