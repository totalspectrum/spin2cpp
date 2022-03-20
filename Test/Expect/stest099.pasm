pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_copy1
	mov	_var01, #0
	sub	arg03, #1
	cmps	arg03, #0 wc
	negc	_var02, #1
	add	arg03, _var02
LR__0001
	rdbyte	_var03, arg02
	wrbyte	_var03, arg01
	add	_var01, _var02
	cmp	_var01, arg03 wz
	add	arg02, #1
	add	arg01, #1
 if_ne	jmp	#LR__0001
_copy1_ret
	ret

_copy2
	cmp	arg03, #0 wz
 if_e	jmp	#LR__0003
LR__0002
	rdbyte	_var01, arg02
	wrbyte	_var01, arg01
	add	arg02, #1
	add	arg01, #1
	djnz	arg03, #LR__0002
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
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
