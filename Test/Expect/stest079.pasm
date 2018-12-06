pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_tx
	mov	_var01, #4
LR__0001
	mov	outa, arg1
	add	arg1, #1
	djnz	_var01, #LR__0001
_tx_ret
	ret

_str
	mov	_str_s, arg1
LR__0002
	rdbyte	_str_c, _str_s wz
	add	_str_s, #1
 if_e	jmp	#LR__0003
	mov	arg1, _str_c
	call	#_tx
	jmp	#LR__0002
LR__0003
_str_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_str_c
	res	1
_str_s
	res	1
_var01
	res	1
arg1
	res	1
	fit	496
