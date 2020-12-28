pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setsize
	shr	arg02, #2
	add	ptr__dat__, #4
	wrlong	arg02, ptr__dat__
	sub	ptr__dat__, #4
_setsize_ret
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
arg01
	res	1
arg02
	res	1
	fit	496
