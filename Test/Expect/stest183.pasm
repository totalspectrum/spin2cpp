pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setval
	wrlong	arg02, arg01
_setval_ret
	ret

_setg
	wrlong	arg01, ptr__dat__
_setg_ret
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
	byte	$00, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
