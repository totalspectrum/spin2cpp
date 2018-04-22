PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_ex
	mov	_var_b, #0
	cmps	arg1, #0 wc,wz
 if_b	neg	_var_b, #1
	wrlong	_var_b, objptr
	mov	result1, arg1
_ex_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
_var_b
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
