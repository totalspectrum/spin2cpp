DAT
	org	0
stest029_setpin
	or	OUTA, #2
stest029_setpin_ret
	ret

stest029_clrpin
	andn	OUTA, #2
stest029_clrpin_ret
	ret

