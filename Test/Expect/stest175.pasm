pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_calcresult
	cmp	arg01, #1 wz
 if_e	jmp	#LR__0001
	cmp	arg01, imm_4294967295_ wz
 if_e	jmp	#LR__0002
	jmp	#LR__0003
LR__0001
	mov	_var01, arg02
	add	_var01, arg03
	mov	_var02, _var01
	mov	result1, _var02
	jmp	#_calcresult_ret
LR__0002
	mov	_var01, arg02
	sub	_var01, arg03
	mov	_var03, _var01
	mov	result1, _var03
	jmp	#_calcresult_ret
LR__0003
	mov	result1, arg02
_calcresult_ret
	ret

imm_4294967295_
	long	-1
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
