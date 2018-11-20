con
	all_pin_mask = 252
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_pushdata
	andn	outa, #252
	shl	arg1, #2
	or	outa, arg1
_pushdata_ret
	ret

_all_pin_low
	andn	outa, #252
_all_pin_low_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
	fit	496
