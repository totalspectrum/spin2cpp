DAT
	org	0
stest029_setpin
	or	OUTA, #2
stest029_setpin_ret
	ret

stest029_clrpin
	and	OUTA, imm_4294967293_
stest029_clrpin_ret
	ret

imm_4294967293_
	long	-3
