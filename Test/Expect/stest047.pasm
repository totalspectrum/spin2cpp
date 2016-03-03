DAT
	org	0
addone
	add	arg1_, #1
	mov	result_, arg1_
addone_ret
	ret

arg1_
	long	0
result_
	long	0
