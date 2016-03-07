CON
	incval = 15
DAT
	org	0

_inc
	add	arg1_, #incval
	mov	result_, arg1_
_inc_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
