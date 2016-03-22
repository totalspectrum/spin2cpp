DAT
	org	0

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

_substest01_get
	rdlong	result1, objptr
_substest01_get_ret
	ret

_substest01_add
	rdlong	result1, objptr
	add	result1, arg1
	wrlong	result1, objptr
_substest01_add_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
objptr
	long	@@@objmem
result1
	long	0
	fit	496
	long
objmem
	long	$00000000
	long	$00000000
	long	$00000000
