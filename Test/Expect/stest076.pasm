PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_shiftout
	mov	_var_00, #1
	shl	_var_00, arg2
	mov	_var_01, #8
L__0003
	shr	arg1, #1 wc
	muxc	OUTA, _var_00
	djnz	_var_01, #L__0003
_shiftout_ret
	ret

_var_00
	long	0
_var_01
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
