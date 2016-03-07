DAT
	org	0

_sum
	add	arg1_, arg2_
	mov	result_, arg1_
_sum_ret
	ret

_inc
	add	arg1_, #1
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
