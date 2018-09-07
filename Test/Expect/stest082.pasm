PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_incby
	shl	arg1, #2
	add	arg1, objptr
	rdlong	_var_04, arg1
	add	_var_04, arg2
	wrlong	_var_04, arg1
_incby_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_var_04
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
