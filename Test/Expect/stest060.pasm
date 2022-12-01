pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	add	objptr, #4
	rdlong	result1, objptr
	add	result1, #4
	wrlong	result1, objptr
	add	objptr, #4
	rdlong	result1, objptr
	add	result1, #3
	wrlong	result1, objptr
	sub	objptr, #8
_demo_ret
	ret

_substest01_add
	rdlong	result1, objptr
	add	result1, arg01
	wrlong	result1, objptr
_substest01_add_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[3]
	org	COG_BSS_START
arg01
	res	1
	fit	496
