pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ex
	mov	_var01, #0
	cmps	arg01, #0 wc,wz
 if_b	neg	_var01, #1
	wrlong	_var01, objptr
	mov	result1, arg01
_ex_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
