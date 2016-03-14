DAT
	org	0

_blinky
	mov	arg2_, ptr__dat__
	mov	arg1_, #8
	mov	arg3_, #240
	call	#__system___coginit
_blinky_ret
	ret

__system___coginit
	and	arg3_, imm_65532_
	shl	arg3_, #16
	and	arg2_, imm_65532_
	shl	arg2_, #2
	or	arg3_, arg2_
	and	arg1_, #15
	or	arg3_, arg1_
	coginit	arg3_ wr
	mov	result_, arg3_
__system___coginit_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
imm_65532_
	long	65532
ptr__dat__
	long	@@@_dat_
result_
	long	0
	fit	496
	long
_dat_
	byte	$f0,$19,$bc,$a0,$04,$18,$fc,$28
	byte	$01,$1a,$fc,$a0,$0c,$1a,$bc,$2c
	byte	$0d,$ec,$bf,$a0,$0d,$e8,$bf,$a0
	byte	$00,$1c,$fc,$08,$0e,$1e,$bc,$a0
	byte	$f1,$1f,$bc,$80,$0e,$1e,$bc,$f8
	byte	$0d,$e8,$bf,$6c,$09,$00,$7c,$5c
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
