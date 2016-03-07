DAT
	org	0

_addone
	add	arg1_, #1
	mov	result_, arg1_
_addone_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
