DAT
	org	0

indsum
	rdlong	result_, arg1_
	rdlong	indsum_tmp003_, arg2_
	add	result_, indsum_tmp003_
indsum_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
indsum_tmp003_
	long	0
result_
	long	0
