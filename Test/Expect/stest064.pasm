DAT
	org	0

_getptr
	mov	result_, ptr__objmem_
	add	result_, #16
_getptr_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
ptr__objmem_
	long	@@@_objmem
result_
	long	0
	long
_objmem
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
