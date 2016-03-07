DAT
	org	0

_clear0
	andn	OUTA, #1
_clear0_ret
	ret

_clearpin
	mov	clearpin_tmp001_, #1
	shl	clearpin_tmp001_, arg1_
	andn	OUTA, clearpin_tmp001_
_clearpin_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
clearpin_tmp001_
	long	0
result_
	long	0
