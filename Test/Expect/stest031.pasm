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
	mov	_var_01, #1
	shl	_var_01, arg1
	andn	OUTA, _var_01
_clearpin_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
	res	1
arg1
	res	1
	fit	496
