pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	mov	_var01, arg01
	add	_var01, #1
	sub	arg01, #1
	mov	outa, _var01
	mov	dira, arg01
_blah_ret
	ret

_main
	neg	_main_i_0003, #1
LR__0001
	cmps	_main_i_0003, #2 wc
 if_ae	jmp	#LR__0002
	mov	arg01, _main_i_0003
	call	#_blah
	add	_main_i_0003, #1
	jmp	#LR__0001
LR__0002
_main_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_main_i_0003
	res	1
_var01
	res	1
arg01
	res	1
	fit	496
