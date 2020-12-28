pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_sumit
	cmp	arg01, arg02 wz
 if_ne	mov	result1, arg03
 if_e	sub	arg01, arg02
 if_e	mov	result1, arg01
_sumit_ret
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
arg03
	res	1
	fit	496
