CON
	all_pin_mask = 252
PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_pushData
	andn	OUTA, #252
	shl	arg1, #2
	or	OUTA, arg1
_pushData_ret
	ret

_all_pin_low
	andn	OUTA, #252
_all_pin_low_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
