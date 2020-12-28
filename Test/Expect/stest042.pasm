pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_select
	cmp	arg01, #0 wz
 if_ne	mov	_var01, arg02
 if_e	add	arg03, #2
 if_e	mov	_var01, arg03
	mov	result1, _var01
_select_ret
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
arg02
	res	1
arg03
	res	1
	fit	496
