pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blink
	mov	_var01, #1
	shl	_var01, arg01
	cmp	arg02, #0 wz
 if_e	jmp	#LR__0002
LR__0001
	xor	outa, _var01
	djnz	arg02, #LR__0001
LR__0002
	rdlong	result1, ptr__dat__
_blink_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$44, $33, $22, $11
	long	@@@_dat_ + 4
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
