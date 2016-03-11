DAT
	org	0

_setpin
	or	OUTA, #2
_setpin_ret
	ret

_clrpin
	andn	OUTA, #2
_clrpin_ret
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
