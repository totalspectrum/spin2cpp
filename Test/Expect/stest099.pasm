pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_copy1
	mov	_var01, #0
	sub	arg3, #1
	cmps	arg3, #0 wc,wz
 if_a	mov	_var02, #1
 if_be	neg	_var02, #1
	add	arg3, _var02
LR__0001
	mov	_var03, arg2
	add	_var03, #1
	rdbyte	_var04, arg2
	mov	arg2, _var03
	wrbyte	_var04, arg1
	add	_var01, _var02
	cmp	_var01, arg3 wz
	add	arg1, #1
 if_ne	jmp	#LR__0001
_copy1_ret
	ret

_copy2
	cmp	arg3, #0 wz
 if_e	jmp	#LR__0003
LR__0002
	mov	_var01, arg2
	add	_var01, #1
	rdbyte	_var02, arg2
	mov	arg2, _var01
	wrbyte	_var02, arg1
	add	arg1, #1
	djnz	arg3, #LR__0002
LR__0003
_copy2_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496
