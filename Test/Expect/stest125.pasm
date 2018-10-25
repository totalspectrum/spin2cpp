PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_sfetch
	rdbyte	result1, arg1
	shl	result1, #24
	sar	result1, #24
_sfetch_ret
	ret

_ufetch
	rdbyte	result1, arg1
_ufetch_ret
	ret

_ushift
	shr	arg1, arg2
	mov	result1, arg1
_ushift_ret
	ret

_sshift
	sar	arg1, arg2
	mov	result1, arg1
_sshift_ret
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
