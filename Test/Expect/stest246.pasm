pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch
	add	arg01, ptr__dat__
	rdbyte	result1, arg01
_fetch_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$61, $62, $63, $00
	org	COG_BSS_START
arg01
	res	1
	fit	496
