pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_testit
	test	arg01, #4 wz
 if_ne	mov	result1, arg02
 if_e	mov	result1, #0
_testit_ret
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
