pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_calcresult
	mov	_var01, arg01
	mov	_var02, arg02
	mov	_var03, arg03
	add	_var01, #3
	max	_var01, #8
	add	_var01, ptr_L__0008_
	jmp	_var01
LR__0001
	jmp	#LR__0005
	jmp	#LR__0007
	jmp	#LR__0004
	jmp	#LR__0007
	jmp	#LR__0003
	jmp	#LR__0002
	jmp	#LR__0007
	jmp	#LR__0006
	jmp	#LR__0007
LR__0002
LR__0003
	mov	result1, _var02
	add	result1, _var03
	jmp	#_calcresult_ret
LR__0004
	mov	result1, _var02
	sub	result1, _var03
	jmp	#_calcresult_ret
LR__0005
	neg	result1, _var02
	jmp	#_calcresult_ret
LR__0006
	mov	result1, _var03
	jmp	#_calcresult_ret
LR__0007
	mov	result1, _var02
_calcresult_ret
	ret

ptr_L__0008_
	long	LR__0001
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
