pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setit
	mov	_var01, #0
	mov	_var02, #8
	wrlong	_var01, ptr__dat__
	add	ptr__dat__, #4
	wrlong	_var02, ptr__dat__
	sub	ptr__dat__, #4
_setit_ret
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
	fit	496
