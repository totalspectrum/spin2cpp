CON
	incval = 15
DAT
	org	0
inc
	add	inc_x_, #incval
	mov	result_, inc_x_
inc_ret
	ret

inc_x_
	long	0
result_
	long	0
