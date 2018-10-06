CON
	_clkfreq = 80000000
	_clkmode = 1032
	txpin = 30
	baud = 115200
	txmask = 1073741824
	bitcycles = 694
PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_serchar
	mov	_serchar_v, arg1
	or	OUTA, imm_1073741824_
	or	DIRA, imm_1073741824_
	or	_serchar_v, #256
	shl	_serchar_v, #1
	mov	_serchar_waitcycles, CNT
	mov	_serchar__idx__0001, #10
LR__0001
	add	_serchar_waitcycles, imm_694_
	mov	arg1, _serchar_waitcycles
	waitcnt	arg1, #0
	shr	_serchar_v, #1 wc
	muxc	OUTA, imm_1073741824_
	djnz	_serchar__idx__0001, #LR__0001
_serchar_ret
	ret

imm_1073741824_
	long	1073741824
imm_694_
	long	694
COG_BSS_START
	fit	496
	org	COG_BSS_START
_serchar__idx__0001
	res	1
_serchar_v
	res	1
_serchar_waitcycles
	res	1
arg1
	res	1
	fit	496
