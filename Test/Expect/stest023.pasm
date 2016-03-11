DAT
	org	0

_blah
	mov	result_, #3
_blah_ret
	ret

_bar
	add	arg1_, #1
	mov	result_, arg1_
_bar_ret
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
