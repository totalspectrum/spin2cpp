DAT
	org	0
clear0
	andn	OUTA, #1
clear0_ret
	ret

clearpin
	mov	clearpin_tmp001_, #1
	shl	clearpin_tmp001_, arg1_
	andn	OUTA, clearpin_tmp001_
clearpin_ret
	ret

arg1_
	long	0
clearpin_tmp001_
	long	0
