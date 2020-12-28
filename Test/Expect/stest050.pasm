con
	all_pin_mask = 252
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_pushData
	andn	outa, #252
	shl	arg01, #2
	or	outa, arg01
_pushData_ret
	ret

_all_pin_low
	andn	outa, #252
_all_pin_low_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
