PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

__call_method
	wrlong	objptr, sp
	add	sp, #4
	mov	objptr, arg1
	mov	arg1, arg3
	call	arg2
	sub	sp, #4
	rdlong	objptr, sp
__call_method_ret
	ret

__basic_print_char
	mov	__basic_print_char_h, arg1
	mov	__basic_print_char_c, arg2
	mov	_basic_print_char_tmp001_, __basic_print_char_h
	shl	_basic_print_char_tmp001_, #2
	mov	_basic_print_char_tmp002_, ptr__dat__
	add	_basic_print_char_tmp001_, _basic_print_char_tmp002_
	rdlong	__basic_print_char_t, _basic_print_char_tmp001_ wz
 if_e	jmp	#__basic_print_char_ret
	rdlong	__basic_print_char_o, __basic_print_char_t
	add	__basic_print_char_t, #4
	rdlong	__basic_print_char_f, __basic_print_char_t wz
 if_ne	jmp	#LR__0001
	mov	arg1, __basic_print_char_c
	call	#__mytx
	jmp	#LR__0002
LR__0001
	mov	arg1, __basic_print_char_o
	mov	arg2, __basic_print_char_f
	mov	arg3, __basic_print_char_c
	call	#__call_method
LR__0002
__basic_print_char_ret
	ret

__mytx
	mov	_var_01, #0
LR__0003
	mov	_var_02, arg1
	or	_var_02, _var_01
	mov	OUTA, _var_02
	add	_var_01, #1
	cmps	_var_01, #5 wc,wz
 if_b	jmp	#LR__0003
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
__basic_print_char_c
	res	1
__basic_print_char_f
	res	1
__basic_print_char_h
	res	1
__basic_print_char_o
	res	1
__basic_print_char_t
	res	1
_basic_print_char_tmp001_
	res	1
_basic_print_char_tmp002_
	res	1
_var_01
	res	1
_var_02
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496
