pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_square_wave_cog
	rdlong	_var01, arg01
	add	ptr__dat__, #8
	wrlong	_var01, ptr__dat__
	add	ptr__dat__, #8
	rdlong	_var01, ptr__dat__
	sub	ptr__dat__, #16
	cmp	frqa, _var01 wz
 if_ne	add	ptr__dat__, #16
 if_ne	rdlong	frqa, ptr__dat__
 if_ne	sub	ptr__dat__, #16
_square_wave_cog_ret
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
	byte	$00[20]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
