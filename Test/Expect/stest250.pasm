pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_doresult
	sar	arg01, #24
	and	arg01, #7
	test	arg01, #2 wz
 if_e	add	arg02, #1
	test	arg01, #4 wz
 if_e	sub	arg02, #1
	mov	result1, arg02
_doresult_ret
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
