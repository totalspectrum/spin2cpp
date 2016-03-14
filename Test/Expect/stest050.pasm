CON
	all_pin_mask = 252
DAT
	org	0

_pushData
	andn	OUTA, #all_pin_mask
	shl	arg1, #2
	or	OUTA, arg1
_pushData_ret
	ret

_all_pin_low
	andn	OUTA, #all_pin_mask
_all_pin_low_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
	fit	496
