pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getptr
	mov	result1, objptr
	add	result1, #16
_getptr_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[8]
	org	COG_BSS_START
	fit	496
