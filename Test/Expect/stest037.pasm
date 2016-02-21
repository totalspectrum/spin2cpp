DAT
	org	0
fetch
	shl	fetch_i_, #1
	add	fetch_i_, fetch_x_
	rdword	result_, fetch_i_
fetch_ret
	ret

fetch_i_
	long	0
fetch_x_
	long	0
result_
	long	0
