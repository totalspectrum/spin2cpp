CON
	_clkfreq = 80000000
	_clkmode = 1032
	txpin = 30
	baud = 115200
	txmask = 1073741824
	bitcycles = 694
DAT
	org	0

_serchar
	mov	_serchar_val, arg1_
	or	OUTA, imm_1073741824_
	or	DIRA, imm_1073741824_
	or	_serchar_val, #256
	shl	_serchar_val, #1
	mov	_serchar_waitcycles, CNT
	mov	_serchar__idx__0000, #10
L_001_
	add	_serchar_waitcycles, imm_694_
	mov	arg1_, _serchar_waitcycles
	waitcnt	arg1_, #0
	test	_serchar_val, #1 wz
 if_ne	or	OUTA, imm_1073741824_
 if_e	andn	OUTA, imm_1073741824_
	shr	_serchar_val, #1
	djnz	_serchar__idx__0000, #L_001_
_serchar_ret
	ret

_serchar__idx__0000
	long	0
_serchar_val
	long	0
_serchar_waitcycles
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
imm_1073741824_
	long	1073741824
imm_694_
	long	694
result_
	long	0
