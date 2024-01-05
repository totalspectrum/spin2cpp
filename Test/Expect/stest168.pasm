pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_bump2
_bump1
_bumppc
	shl	arg02, #16
	sar	arg02, #16
	add	arg01, arg02
	mov	result1, arg01
_bumppc_ret
_bump2_ret
_bump1_ret
	ret



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
