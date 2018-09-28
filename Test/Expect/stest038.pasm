PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test
LR__0001
	mov	_var_00, INA
	sar	_var_00, #1
	test	_var_00, #1 wz
 if_ne	jmp	#LR__0002
	mov	_tmp001_, INA
	sar	_tmp001_, #2
	test	_tmp001_, #1 wz
 if_ne	jmp	#LR__0001
	xor	OUTA, #1
	jmp	#LR__0001
LR__0002
_test_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
_var_00
	res	1
	fit	496
