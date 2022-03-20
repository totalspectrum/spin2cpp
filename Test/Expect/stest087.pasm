pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_update
	mov	_var01, #0
LR__0001
	cmps	_var01, #10 wc
 if_ae	jmp	#LR__0002
	rdlong	_var02, arg01
	add	_var02, #1
	wrlong	_var02, arg01
	add	_var01, #1
	add	arg01, #4
	jmp	#LR__0001
LR__0002
_update_ret
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
