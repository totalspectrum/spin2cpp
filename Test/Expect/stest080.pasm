pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	and	arg01, #15
	mov	_var01, outa
	and	_var01, imm_4294967280_
	or	_var01, arg01
	mov	outa, _var01
_blah_ret
	ret

_onepin
	test	arg01, #1 wz
	muxnz	outa, #16
_onepin_ret
	ret

imm_4294967280_
	long	-16
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
