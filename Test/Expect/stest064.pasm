DAT
	org	0

_getptr
	add	ptr__objmem_, #16
	mov	result_, ptr__objmem_
	sub	ptr__objmem_, #16
_getptr_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
ptr__objmem_
	long	@@@_objmem
result_
	long	0
	fit	496
	long
_objmem
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
