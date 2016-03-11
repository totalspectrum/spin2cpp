DAT
	org	0

_sum
	add	arg1_, arg2_
	mov	result_, arg1_
_sum_ret
	ret

_one
	mov	result_, #1
_one_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
