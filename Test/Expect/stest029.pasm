pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setpin
	or	outa, #2
_setpin_ret
	ret

_clrpin
	andn	outa, #2
_clrpin_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
