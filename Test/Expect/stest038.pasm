PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test
LR__0001
	mov	_var_01, INA
	sar	_var_01, #1
	test	_var_01, #1 wz
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

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
_var_01
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
