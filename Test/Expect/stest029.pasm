PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_setpin
	or	OUTA, #2
_setpin_ret
	ret

_clrpin
	andn	OUTA, #2
_clrpin_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
