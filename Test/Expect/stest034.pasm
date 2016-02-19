DAT
	org	0
stest034_indsum
	rdlong	result_, stest034_indsum_x_
	rdlong	stest034_indsum_tmp003_, stest034_indsum_y_
	add	result_, stest034_indsum_tmp003_
stest034_indsum_ret
	ret

result_
	long	0
stest034_indsum_tmp003_
	long	0
stest034_indsum_x_
	long	0
stest034_indsum_y_
	long	0
