DAT
	org	0

_shiftout
	mov	_shiftout__idx__0015, #32
L_039_
	mov	_shiftout__mask_0014, #1
	shl	_shiftout__mask_0014, arg2
	shr	arg1, #1 wc
	muxc	OUTA, _shiftout__mask_0014
	djnz	_shiftout__idx__0015, #L_039_
_shiftout_ret
	ret

_shiftout__idx__0015
	long	0
_shiftout__mask_0014
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
