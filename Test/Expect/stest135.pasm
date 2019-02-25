pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zero
	mov	_var01, arg01
	cmps	arg02, #0 wc,wz
 if_be	jmp	#LR__0002
	mov	_var02, arg02
LR__0001
	mov	arg01, #0
	wrword	arg01, _var01
	add	_var01, #2
	djnz	_var02, #LR__0001
LR__0002
_zero_ret
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
arg02
	res	1
	fit	496
