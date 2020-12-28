pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_clear0
	andn	outa, #1
_clear0_ret
	ret

_clearpin
	mov	_var01, #1
	shl	_var01, arg01
	andn	outa, _var01
_clearpin_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
