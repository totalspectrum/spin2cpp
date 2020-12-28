pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_indsum
	rdlong	result1, arg01
	rdlong	_var01, arg02
	add	result1, _var01
_indsum_ret
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
_var01
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
