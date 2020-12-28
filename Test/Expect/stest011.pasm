pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test
	mov	dira, #1
	mov	outa, #1
_test_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
