pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

__call_method
	wrlong	objptr, sp
	add	sp, #4
	mov	objptr, arg01
	mov	arg01, arg03
	call	arg02
	sub	sp, #4
	rdlong	objptr, sp
__call_method_ret
	ret

__basic_print_char
	mov	__basic_print_char__csetype__0000, arg01
	mov	__basic_print_char_c, arg02
	shl	__basic_print_char__csetype__0000, #2
	add	__basic_print_char__csetype__0000, ptr__dat__
	rdlong	__basic_print_char_t, __basic_print_char__csetype__0000 wz
 if_e	jmp	#__basic_print_char_ret
	rdlong	__basic_print_char__csetype__0000, __basic_print_char_t
	add	__basic_print_char_t, #4
	rdlong	__basic_print_char_t, __basic_print_char_t wz
 if_ne	jmp	#LR__0001
	mov	arg01, __basic_print_char_c
	call	#__mytx
	jmp	#LR__0002
LR__0001
	mov	arg01, __basic_print_char__csetype__0000
	mov	arg02, __basic_print_char_t
	mov	arg03, __basic_print_char_c
	call	#__call_method
LR__0002
__basic_print_char_ret
	ret

__mytx
	mov	_var01, #0
LR__0010
	mov	_var02, arg01
	or	_var02, _var01
	mov	outa, _var02
	add	_var01, #1
	cmps	_var01, #5 wc
 if_b	jmp	#LR__0010
__mytx_ret
	ret

objptr
	long	@@@objmem
ptr__dat__
	long	@@@_dat_
result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[32]
objmem
	long	0[0]
stackspace
	long	0[1]
	org	COG_BSS_START
__basic_print_char__csetype__0000
	res	1
__basic_print_char_c
	res	1
__basic_print_char_t
	res	1
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
