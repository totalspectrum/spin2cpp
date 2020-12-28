pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_func
	rdlong	outa, objptr
	wrlong	arg01, objptr
_func_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
arg01
	res	1
	fit	496
