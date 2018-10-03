PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_ismagic
	cmp	arg1, #511 wz
 if_e	mov	_var_02, #1
 if_ne	mov	_var_02, #0
	mov	result1, _var_02
_ismagic_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_02
	res	1
arg1
	res	1
	fit	496
