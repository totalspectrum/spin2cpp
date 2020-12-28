pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_pokeb
	wrbyte	arg02, arg01
_pokeb_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
