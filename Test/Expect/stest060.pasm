DAT
	org	0

_demo
	mov	arg1_, #4
	add	ptr__objmem_, #4
	call	#_stestinc_add
	mov	arg1_, #3
	add	ptr__objmem_, #4
	call	#_stestinc_add
	sub	ptr__objmem_, #8
_demo_ret
	ret

_stestinc_get
	rdlong	result_, ptr__objmem_
_stestinc_get_ret
	ret

_stestinc_add
	rdlong	stestinc_add_tmp001_, ptr__objmem_
	add	stestinc_add_tmp001_, arg1_
	wrlong	stestinc_add_tmp001_, ptr__objmem_
	rdlong	result_, ptr__objmem_
_stestinc_add_ret
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
stestinc_add_tmp001_
	long	0
	long
_objmem
	byte	$00,$00,$00,$00,$00,$00,$00,$00
	byte	$00,$00,$00,$00
