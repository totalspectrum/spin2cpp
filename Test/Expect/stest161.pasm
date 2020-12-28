pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test2
	or	outa, #8
	or	outa, #16
_test2_ret
	ret

_test1
	or	outa, #8
_test1_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
