DAT
	org	0

_demo
	mov	arg1_, #4
	add	ptr__objmem_, #4
	call	#_substest01_add
	mov	arg1_, #3
	add	ptr__objmem_, #4
	call	#_substest01_add
	sub	ptr__objmem_, #8
_demo_ret
	ret

_substest01_get
	rdlong	result_, ptr__objmem_
_substest01_get_ret
	ret

_substest01_add
	rdlong	substest01_add_tmp001_, ptr__objmem_
	add	substest01_add_tmp001_, arg1_
	wrlong	substest01_add_tmp001_, ptr__objmem_
	rdlong	result_, ptr__objmem_
_substest01_add_ret
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
substest01_add_tmp001_
	long	0
	long
_objmem
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00
