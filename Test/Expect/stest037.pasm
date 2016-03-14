DAT
	org	0

_fetch
	shl	arg2_, #1
	add	arg1_, arg2_
	rdword	result_, arg1_
_fetch_ret
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
	fit	496
