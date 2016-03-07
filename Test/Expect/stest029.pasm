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
arg2_
	long	0
arg3_
	long	0
result_
	long	0
