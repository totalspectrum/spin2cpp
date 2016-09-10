PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_shiftout
	mov	_var_03, #1
	shl	_var_03, arg2
	mov	_var_04, #8
L__0003
	shr	arg1, #1 wc
	muxc	OUTA, _var_03
	djnz	_var_04, #L__0003
_shiftout_ret
	ret

_var_03
	long	0
_var_04
	long	0
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
