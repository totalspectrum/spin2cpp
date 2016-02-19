DAT
	org	0
stest037_fetch
	shl	stest037_fetch_i_, #1
	add	stest037_fetch_i_, stest037_fetch_x_
	rdword	result_, stest037_fetch_i_
stest037_fetch_ret
	ret

result_
	long	0
stest037_fetch_i_
	long	0
stest037_fetch_x_
	long	0
