DAT
	org	0

_func
	add	ptr__objmem_, #8
	rdlong	func_tmp001_, ptr__objmem_
	add	ptr__objmem_, #4
	rdbyte	func_tmp003_, ptr__objmem_
	add	func_tmp001_, func_tmp003_
	sub	ptr__objmem_, #8
	wrlong	func_tmp001_, ptr__objmem_
	sub	ptr__objmem_, #4
_func_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
func_tmp001_
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
