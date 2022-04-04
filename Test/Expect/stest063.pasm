pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blinky
	mov	arg02, ptr__dat__
	mov	arg03, imm_15728640_
	and	arg02, imm_65532_
	shl	arg02, #2
	or	arg03, arg02
	or	arg03, #8
	coginit	arg03 wc,wr
_blinky_ret
	ret

imm_15728640_
	long	15728640
imm_65532_
	long	65532
ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$f0, $19, $bc, $a0, $04, $18, $fc, $28, $01, $1a, $fc, $a0, $0c, $1a, $bc, $2c
	byte	$0d, $ec, $bf, $a0, $0d, $e8, $bf, $a0, $00, $1c, $fc, $08, $0e, $1e, $bc, $a0
	byte	$f1, $1f, $bc, $80, $0e, $1e, $bc, $f8, $0d, $e8, $bf, $6c, $09, $00, $7c, $5c
	byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
