DAT
	org	0

_shiftout
	mov	_shiftout__mask_0001, #1
	shl	_shiftout__mask_0001, arg2
	mov	_shiftout__idx__0002, #32
L__0003
	shr	arg1, #1 wc
	muxc	OUTA, _shiftout__mask_0001
	djnz	_shiftout__idx__0002, #L__0003
_shiftout_ret
	ret

_shiftout__idx__0002
	long	0
_shiftout__mask_0001
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
