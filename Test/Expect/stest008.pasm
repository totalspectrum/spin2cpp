PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_sum
	add	arg1, arg2
	mov	result1, arg1
_sum_ret
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
	fit	496
