CON
	_clkfreq = 80000000
	_clkmode = 1032
	txpin = 30
	baud = 115200
	txmask = 1073741824
	bitcycles = 694
DAT
	org	0
serchar
	or	OUTA, imm_1073741824_
	or	DIRA, imm_1073741824_
	or	arg1_, #256
	shl	arg1_, #1
	mov	serchar_waitcycles_, CNT
	mov	serchar__idx__0000_, #10
L_001_
	add	serchar_waitcycles_, imm_694_
	waitcnt	serchar_waitcycles_, #0
	mov	serchar_tmp001_, arg1_
	and	serchar_tmp001_, #1 wz
 if_ne	or	OUTA, imm_1073741824_
 if_e	andn	OUTA, imm_1073741824_
	shr	arg1_, #1
	djnz	serchar__idx__0000_,#L_001_
serchar_ret
	ret

arg1_
	long	0
imm_1073741824_
	long	1073741824
imm_694_
	long	694
serchar__idx__0000_
	long	0
serchar_tmp001_
	long	0
serchar_waitcycles_
	long	0
