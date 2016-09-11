PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_clear0
	andn	OUTA, #1
_clear0_ret
	ret

_clearpin
	mov	_tmp001_, #1
	shl	_tmp001_, arg1
	andn	OUTA, _tmp001_
_clearpin_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
