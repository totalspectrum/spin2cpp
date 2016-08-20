PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_shiftout
	mov	_var__mask_0001, #1
	shl	_var__mask_0001, arg2
	mov	_var__idx__0002, #32
L__0003
	shr	arg1, #1 wc
	muxc	OUTA, _var__mask_0001
	djnz	_var__idx__0002, #L__0003
_shiftout_ret
	ret

_var__idx__0002
	long	0
_var__mask_0001
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
