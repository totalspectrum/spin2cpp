pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	outa, #4
	mov	_var01, ptr__dat__
	add	_var01, #2
	mov	outb, _var01
_main_ret
	ret

ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$01, $00, $00, $00, $2a, $00, $00, $00
	org	COG_BSS_START
_var01
	res	1
	fit	496
