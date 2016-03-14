DAT
	org	0

_indsum
	rdlong	result1, arg1
	rdlong	indsum_tmp002_, arg2
	add	result1, indsum_tmp002_
_indsum_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
indsum_tmp002_
	long	0
result1
	long	0
	fit	496
