DAT
	org	0
addone
	add	addone_x_, #1
	mov	result_, addone_x_
addone_ret
	ret

addone_x_
	long	0
result_
	long	0
