DAT
	org	0

_demo
	mov	arg1, #4
	add	objptr, #4
	call	#_substest01_add
	mov	arg1, #3
	add	objptr, #4
	call	#_substest01_add
	sub	objptr, #8
_demo_ret
	ret

_substest01_get
	rdlong	result1, objptr
_substest01_get_ret
	ret

_substest01_add
	rdlong	substest01_add_tmp001_, objptr
	add	substest01_add_tmp001_, arg1
	wrlong	substest01_add_tmp001_, objptr
	rdlong	result1, objptr
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
substest01_add_tmp001_
	long	0
	fit	496
	long
objmem
	long	$00000000
	long	$00000000
	long	$00000000
