pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_sfetch
	rdbyte	result1, arg01
	shl	result1, #24
	sar	result1, #24
_sfetch_ret
	ret

_ufetch
	rdbyte	result1, arg01
_ufetch_ret
	ret

_ushift
	shr	arg01, arg02
	mov	result1, arg01
_ushift_ret
	ret

_sshift
	sar	arg01, arg02
	mov	result1, arg01
_sshift_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
