pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setup
	shl	arg01, #2
	add	arg01, objptr
	add	objptr, #40
	wrlong	arg01, objptr
	add	objptr, #4
	wrlong	arg01, objptr
	sub	objptr, #44
_setup_ret
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
	long	0[12]
	org	COG_BSS_START
arg01
	res	1
	fit	496
