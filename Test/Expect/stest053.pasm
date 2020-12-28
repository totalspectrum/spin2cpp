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
	mov	_serchar_val, arg01
	or	outa, imm_1073741824_
	or	dira, imm_1073741824_
	or	_serchar_val, #256
	shl	_serchar_val, #1
	mov	_serchar_waitcycles, cnt
	mov	_serchar__idx__0000, #10
LR__0001
	add	_serchar_waitcycles, imm_694_
	mov	arg01, _serchar_waitcycles
	waitcnt	arg01, #0
	shr	_serchar_val, #1 wc
	muxc	outa, imm_1073741824_
	djnz	_serchar__idx__0000, #LR__0001
_serchar_ret
	ret

__lockreg
	long	0
imm_1073741824_
	long	1073741824
imm_694_
	long	694
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
_serchar__idx__0000
	res	1
_serchar_val
	res	1
_serchar_waitcycles
	res	1
arg01
	res	1
	fit	496
