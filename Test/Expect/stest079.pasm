pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_tx
	mov	_var01, #4
LR__0001
	mov	outa, arg01
	add	arg01, #1
	djnz	_var01, #LR__0001
_tx_ret
	ret

_str
	mov	_var01, arg01
LR__0010
	rdbyte	arg01, _var01 wz
	add	_var01, #1
 if_e	jmp	#LR__0012
	mov	_var02, #4
LR__0011
	mov	outa, arg01
	add	arg01, #1
	djnz	_var02, #LR__0011
	jmp	#LR__0010
LR__0012
_str_ret
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
