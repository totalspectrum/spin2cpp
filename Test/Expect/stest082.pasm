pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_incby
	shl	arg1, #2
	add	arg1, objptr
	rdlong	_var01, arg1
	add	_var01, arg2
	wrlong	_var01, arg1
_incby_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_var01
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
