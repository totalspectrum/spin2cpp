PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_preclock
	mov	_var_01, #1
	shl	_var_01, arg1
	or	OUTA, _var_01
	or	OUTA, _var_01
_preclock_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
	res	1
arg1
	res	1
	fit	496
