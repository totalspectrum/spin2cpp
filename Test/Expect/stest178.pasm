pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_set1
	mov	_var01, ptr__dat__
	add	_var01, imm_780_
	wrbyte	arg01, _var01
_set1_ret
	ret

_set2
	mov	_var01, ptr__dat__
	add	_var01, imm_780_
	wrbyte	arg01, _var01
_set2_ret
	ret

__lockreg
	long	0
imm_780_
	long	780
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[764]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
