pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_doit
	mov	_var01, arg01
LR__0001
	cmp	_var01, arg01 wc,wz
 if_be	mov	_var02, #1
 if_be	shl	_var02, _var01
 if_be	or	outa, _var02
 if_be	sub	_var01, #1
 if_be	jmp	#LR__0001
_doit_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
	fit	496
