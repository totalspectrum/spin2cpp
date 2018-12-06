pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_shiftout
	mov	_var01, #1
	shl	_var01, arg2
	mov	_var02, #32
LR__0001
	shr	arg1, #1 wc
	muxc	outa, _var01
	djnz	_var02, #LR__0001
_shiftout_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
