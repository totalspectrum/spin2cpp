CON
	incval = 15
DAT
	org	0

inc
	add	arg1_, #incval
	mov	result_, arg1_
inc_ret
	ret

arg1_
	long	0
result_
	long	0
