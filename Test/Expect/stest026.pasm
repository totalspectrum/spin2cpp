DAT
	org	0

test1
	mov	test1_tmp001_, #0
	cmps	arg1_, arg2_ wc,wz
 if_b	neg	test1_tmp001_, #1
	mov	result_, test1_tmp001_
test1_ret
	ret

arg1_
	long	0
arg2_
	long	0
result_
	long	0
test1_tmp001_
	long	0
