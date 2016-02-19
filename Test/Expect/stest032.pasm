CON
	incval = 15
DAT
	org	0
stest032_inc
	add	stest032_inc_x_, #incval
	mov	result_, stest032_inc_x_
stest032_inc_ret
	ret

result_
	long	0
stest032_inc_x_
	long	0
