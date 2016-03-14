DAT
	org	0

_getptr
	add	objptr, #16
	mov	result1, objptr
	sub	objptr, #16
_getptr_ret
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
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
