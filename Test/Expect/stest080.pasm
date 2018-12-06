pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	and	arg1, #15
	mov	_var01, outa
	andn	_var01, #15
	or	_var01, arg1
	mov	outa, _var01
_blah_ret
	ret

_onepin
	test	arg1, #1 wz
	muxnz	outa, #16
_onepin_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg1
	res	1
	fit	496
