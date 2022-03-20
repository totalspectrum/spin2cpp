pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zeroit
	mov	_var01, #1
	add	arg02, #1
LR__0001
	cmps	_var01, arg02 wc
 if_ae	jmp	#LR__0002
	mov	_var02, #0
	wrbyte	_var02, arg01
	add	_var01, #1
	add	arg01, #1
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
arg01
	res	1
arg02
	res	1
	fit	496
