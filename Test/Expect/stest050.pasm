CON
	all_pin_mask = 252
DAT
	org	0

_pushData
	andn	OUTA, #all_pin_mask
	shl	arg1_, #2
	or	OUTA, arg1_
_pushData_ret
	ret

_all_pin_low
	andn	OUTA, #all_pin_mask
_all_pin_low_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
	fit	496
