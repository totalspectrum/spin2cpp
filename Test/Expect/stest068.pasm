PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_shiftout
	mov	_var_03, #1
	shl	_var_03, arg2
	mov	_var_04, #32
L__0010
	shr	arg1, #1 wc
	muxc	OUTA, _var_03
	djnz	_var_04, #L__0010
_shiftout_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_03
	res	1
_var_04
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
