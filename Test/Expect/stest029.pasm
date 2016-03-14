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

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
	fit	496
