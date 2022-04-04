pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_square_wave_cog
	rdlong	arg01, arg01
	add	ptr__dat__, #8
	wrlong	arg01, ptr__dat__
	add	ptr__dat__, #8
	rdlong	arg01, ptr__dat__
	sub	ptr__dat__, #16
	cmp	frqa, arg01 wz
 if_ne	add	ptr__dat__, #16
 if_ne	rdlong	frqa, ptr__dat__
 if_ne	sub	ptr__dat__, #16
_square_wave_cog_ret
	ret

ptr__dat__
	long	@@@_dat_
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[20]
	org	COG_BSS_START
arg01
	res	1
	fit	496
