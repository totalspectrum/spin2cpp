CON
	all_pin_mask = 252
DAT
	org	0

pushData
	andn	OUTA, #all_pin_mask
	shl	arg1_, #2
	or	OUTA, arg1_
pushData_ret
	ret

all_pin_low
	andn	OUTA, #all_pin_mask
all_pin_low_ret
	ret

arg1_
	long	0
result_
	long	0
