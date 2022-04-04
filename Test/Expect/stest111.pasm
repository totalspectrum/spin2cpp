pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_check
	cmp	arg01, #48 wz
 if_ne	cmp	arg01, #49 wz
 if_ne	cmp	arg01, #50 wz
 if_ne	rdlong	_var01, objptr
 if_ne	cmp	arg01, _var01 wz
 if_e	mov	result1, #1
 if_ne	mov	result1, #0
_check_ret
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
_var01
	res	1
arg01
	res	1
	fit	496
