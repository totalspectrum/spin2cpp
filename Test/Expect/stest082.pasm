PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_incby
	mov	_var_03, objptr
	shl	arg1, #2
	add	_var_03, arg1
	rdlong	_var_04, _var_03
	add	_var_04, arg2
	wrlong	_var_04, _var_03
_incby_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	10
	org	COG_BSS_START
_var_03
	res	1
_var_04
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
