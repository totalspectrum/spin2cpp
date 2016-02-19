DAT
	org	0
stest031_clear0
	andn	OUTA, #1
stest031_clear0_ret
	ret

stest031_clearpin
	mov	stest031_clearpin_tmp001_, #1
	shl	stest031_clearpin_tmp001_, stest031_clearpin_pin_
	andn	OUTA, stest031_clearpin_tmp001_
stest031_clearpin_ret
	ret

stest031_clearpin_pin_
	long	0
stest031_clearpin_tmp001_
	long	0
