PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_xorbytes
	cmps	arg3, #0 wz
 if_e	jmp	#LR__0002
LR__0001
	rdbyte	_tmp003_, arg1
	rdbyte	_tmp006_, arg2
	xor	_tmp003_, _tmp006_
	wrbyte	_tmp003_, arg1
	add	arg1, #1
	add	arg2, #1
	djnz	arg3, #LR__0001
LR__0002
_xorbytes_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp003_
	res	1
_tmp006_
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496
