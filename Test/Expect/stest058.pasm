DAT
	org	0

_func
	add	objptr, #8
	rdlong	result1, objptr
	add	objptr, #4
	rdbyte	func_tmp002_, objptr
	sub	objptr, #12
	add	result1, func_tmp002_
_func_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
func_tmp002_
	long	0
objptr
	long	@@@objmem
result1
	long	0
	fit	496
	long
objmem
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
