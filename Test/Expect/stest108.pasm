PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_func
	rdlong	OUTA, objptr
	wrlong	arg1, objptr
_func_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
arg1
	res	1
	fit	496
