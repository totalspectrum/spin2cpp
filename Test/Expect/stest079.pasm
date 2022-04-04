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
	mov	_str_s, arg01
LR__0002
	rdbyte	arg01, _str_s wz
	add	_str_s, #1
 if_e	jmp	#LR__0004
	mov	_tx__idx__0000, #4
LR__0003
	mov	outa, arg01
	add	arg01, #1
	djnz	_tx__idx__0000, #LR__0003
	jmp	#LR__0002
LR__0004
_str_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_str_s
	res	1
_tx__idx__0000
	res	1
_var01
	res	1
arg01
	res	1
	fit	496
