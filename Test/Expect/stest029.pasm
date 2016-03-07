DAT
	org	0

setpin
	or	OUTA, #2
setpin_ret
	ret

clrpin
	andn	OUTA, #2
clrpin_ret
	ret

arg1_
	long	0
