pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_calcresult
	mov	_var01, arg01
	mov	_var02, arg02
	mov	_var03, arg03
	mov	_var04, _var01
	add	_var04, #3
	mov	_var05, _var04
	max	_var05, #8
	mov	_var06, _var05
	shl	_var06, #2
	add	_var06, ptr_L__0009_
	jmp	_var06
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
	mov	_var07, _var02
	add	_var07, _var03
	mov	result1, _var07
	jmp	#_calcresult_ret
LR__0004
	mov	_var07, _var02
	sub	_var07, _var03
	mov	result1, _var07
	jmp	#_calcresult_ret
LR__0005
	mov	_var07, _var02
	neg	_var07, _var07
	mov	result1, _var07
	jmp	#_calcresult_ret
LR__0006
	mov	result1, _var03
	jmp	#_calcresult_ret
LR__0007
	mov	result1, _var02
_calcresult_ret
	ret

ptr_L__0009_
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
_var04
	res	1
_var05
	res	1
_var06
	res	1
_var07
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
