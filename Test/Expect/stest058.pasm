DAT
	org	0

_func
	add	ptr__objmem_, #8
	rdlong	result_, ptr__objmem_
	add	ptr__objmem_, #4
	rdbyte	func_tmp003_, ptr__objmem_
	sub	ptr__objmem_, #12
	add	result_, func_tmp003_
_func_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
func_tmp003_
	long	0
ptr__objmem_
	long	@@@_objmem
result_
	long	0
	long
_objmem
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00,$00,$00,$00,$00
