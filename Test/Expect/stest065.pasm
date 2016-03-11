DAT
	org	0

_set
	add	ptr__objmem_, #9
	rdbyte	set_tmp001_, ptr__objmem_
	sub	ptr__objmem_, #5
	wrlong	set_tmp001_, ptr__objmem_
	sub	ptr__objmem_, #4
_set_ret
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
set_tmp001_
	long	0
	long
_objmem
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00
