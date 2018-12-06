pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_set
	add	objptr, #9
	rdbyte	_var01, objptr
	sub	objptr, #5
	wrlong	_var01, objptr
	sub	objptr, #4
_set_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[3]
	org	COG_BSS_START
_var01
	res	1
	fit	496
