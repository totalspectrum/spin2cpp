pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch
	shl	arg02, #1
	add	arg02, arg01
	rdword	result1, arg02
_fetch_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
