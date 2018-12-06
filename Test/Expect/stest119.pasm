pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zeroit
	mov	_var01, arg1
	mov	_var02, #1
	add	arg2, #1
LR__0001
	cmps	_var02, arg2 wc,wz
 if_ae	jmp	#LR__0002
	mov	arg1, #0
	wrbyte	arg1, _var01
	add	_var02, #1
	add	_var01, #1
	jmp	#LR__0001
LR__0002
	mov	result1, #0
_zeroit_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
