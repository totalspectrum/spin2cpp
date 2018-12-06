pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	_var01, #2
	mov	_var02, #4 wz
LR__0001
	rdlong	_var03, ptr__dat__
	sar	_var03, _var01
	wrlong	_var03, ptr__dat__
	add	ptr__dat__, #4
	rdlong	_var03, ptr__dat__
	shr	_var03, _var01
	wrlong	_var03, ptr__dat__
	sub	ptr__dat__, #4
	djnz	_var02, #LR__0001
_main_ret
	ret

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
_var03
	res	1
	fit	496
