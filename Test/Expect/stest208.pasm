pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_casetest1
	mov	_var01, arg01
	add	_var01, #3
	max	_var01, #4
	add	_var01, ptr_L__0010_
	jmp	_var01
LR__0001
	jmp	#LR__0002
	jmp	#LR__0002
	jmp	#LR__0002
	jmp	#LR__0002
	jmp	#LR__0003
LR__0002
	mov	result1, #1
	jmp	#_casetest1_ret
LR__0003
	mov	result1, #2
_casetest1_ret
	ret

_casetest2
	mov	_var01, arg01
	add	_var01, #3
	max	_var01, #3
	add	_var01, ptr_L__0014_
	jmp	_var01
LR__0004
	jmp	#LR__0005
	jmp	#LR__0005
	jmp	#LR__0005
	jmp	#LR__0006
LR__0005
	mov	result1, #1
	jmp	#_casetest2_ret
LR__0006
	mov	result1, #2
_casetest2_ret
	ret

_casetest3
	mov	_var01, arg01
	max	_var01, #3
	add	_var01, ptr_L__0018_
	jmp	_var01
LR__0007
	jmp	#LR__0008
	jmp	#LR__0008
	jmp	#LR__0008
	jmp	#LR__0009
LR__0008
	mov	result1, #1
	jmp	#_casetest3_ret
LR__0009
	mov	result1, #2
_casetest3_ret
	ret

ptr_L__0010_
	long	LR__0001
ptr_L__0014_
	long	LR__0004
ptr_L__0018_
	long	LR__0007
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
