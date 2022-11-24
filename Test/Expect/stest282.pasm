pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	rdlong	result1, objptr
	add	result1, #1
	wrlong	result1, objptr
	add	objptr, #4
	rdlong	result1, objptr
	add	result1, #2
	wrlong	result1, objptr
	sub	objptr, #4
_demo_ret
	ret

_substest01_add
	rdlong	result1, objptr
	add	result1, arg01
	wrlong	result1, objptr
_substest01_add_ret
	ret

_substest01_inc
	rdlong	result1, objptr
	add	result1, #1
	wrlong	result1, objptr
_substest01_inc_ret
	ret

_substest01_0002_add
	rdlong	result1, objptr
	add	result1, arg01
	wrlong	result1, objptr
_substest01_0002_add_ret
	ret

_substest01_0002_inc
	rdlong	result1, objptr
	add	result1, #2
	wrlong	result1, objptr
_substest01_0002_inc_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[2]
	org	COG_BSS_START
arg01
	res	1
	fit	496
