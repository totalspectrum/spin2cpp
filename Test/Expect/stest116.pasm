pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_index
	sub	arg2, #1
	add	arg2, arg1
	rdbyte	result1, arg2
_index_ret
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
