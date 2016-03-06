DAT
	org	0

fetch
	shl	arg2_, #1
	add	arg2_, arg1_
	rdword	result_, arg2_
fetch_ret
	ret

arg1_
	long	0
arg2_
	long	0
result_
	long	0
