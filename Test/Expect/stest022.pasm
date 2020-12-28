pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ismagic
	cmp	arg01, #511 wz
 if_e	mov	_var01, #1
 if_ne	mov	_var01, #0
	mov	result1, _var01
_ismagic_ret
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
_var01
	res	1
arg01
	res	1
	fit	496
