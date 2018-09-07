PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_preclock
	mov	_var_02, #1
	shl	_var_02, arg1
	or	OUTA, _var_02
	or	OUTA, _var_02
_preclock_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_02
	res	1
arg1
	res	1
	fit	496
