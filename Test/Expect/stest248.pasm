pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_var01, #1
	add	objptr, #4
	wrlong	_var01, objptr
	sub	objptr, #4
_demo_ret
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
	fit	496
