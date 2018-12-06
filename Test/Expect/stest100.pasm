pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_xorbytes
	cmp	arg3, #0 wz
 if_e	jmp	#LR__0002
LR__0001
	rdbyte	_var01, arg1
	rdbyte	_var02, arg2
	xor	_var01, _var02
	wrbyte	_var01, arg1
	add	arg1, #1
	add	arg2, #1
	djnz	arg3, #LR__0001
LR__0002
_xorbytes_ret
	ret

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
arg3
	res	1
	fit	496
