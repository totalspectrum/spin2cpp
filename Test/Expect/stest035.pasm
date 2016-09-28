PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_sum2
	rdlong	result1, arg1
	add	arg1, #4
	rdlong	arg1, arg1
	add	result1, arg1
_sum2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
