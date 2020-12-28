pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	_var01, #4
LR__0001
	rdlong	_var02, ptr__dat__
	sar	_var02, #2
	wrlong	_var02, ptr__dat__
	add	ptr__dat__, #4
	rdlong	_var02, ptr__dat__
	shr	_var02, #2
	wrlong	_var02, ptr__dat__
	sub	ptr__dat__, #4
	djnz	_var01, #LR__0001
_main_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
	fit	496
