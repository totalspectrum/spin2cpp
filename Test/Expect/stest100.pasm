pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_xorbytes
	cmp	arg03, #0 wz
 if_e	jmp	#LR__0002
LR__0001
	rdbyte	_var01, arg02
	rdbyte	_var02, arg01
	xor	_var02, _var01
	wrbyte	_var02, arg01
	add	arg02, #1
	add	arg01, #1
	djnz	arg03, #LR__0001
LR__0002
_xorbytes_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
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
arg03
	res	1
	fit	496
