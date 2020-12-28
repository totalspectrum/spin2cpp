pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_rxcheck
	neg	result1, #1
_rxcheck_ret
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
	fit	496
