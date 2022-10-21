con
	_clkfreq = 80000000
	_clkmode = 1032
	txpin = 30
	baud = 115200
	txmask = 1073741824
	bitcycles = 694
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_serchar
	mov	_var01, arg01
	or	outa, imm_1073741824_
	or	dira, imm_1073741824_
	or	_var01, #256
	shl	_var01, #1
	mov	_var02, cnt
	mov	_var03, #10
LR__0001
	add	_var02, imm_694_
	mov	arg01, _var02
	waitcnt	arg01, #0
	shr	_var01, #1 wc
	muxc	outa, imm_1073741824_
	djnz	_var03, #LR__0001
_serchar_ret
	ret

imm_1073741824_
	long	1073741824
imm_694_
	long	694
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
	fit	496
