DAT
	org	0
indsum
	rdlong	result_, indsum_x_
	rdlong	indsum_tmp003_, indsum_y_
	add	result_, indsum_tmp003_
indsum_ret
	ret

indsum_tmp003_
	long	0
indsum_x_
	long	0
indsum_y_
	long	0
result_
	long	0
