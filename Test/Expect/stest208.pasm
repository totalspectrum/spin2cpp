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
LR__0010
	jmp	#LR__0011
	jmp	#LR__0011
	jmp	#LR__0011
	jmp	#LR__0012
LR__0011
	mov	result1, #1
	jmp	#_casetest2_ret
LR__0012
	mov	result1, #2
_casetest2_ret
	ret

_casetest3
	mov	_var01, arg01
	max	_var01, #3
	add	_var01, ptr_L__0018_
	jmp	_var01
LR__0020
	jmp	#LR__0021
	jmp	#LR__0021
	jmp	#LR__0021
	jmp	#LR__0022
LR__0021
	mov	result1, #1
	jmp	#_casetest3_ret
LR__0022
	mov	result1, #2
_casetest3_ret
	ret

ptr_L__0010_
	long	LR__0001
ptr_L__0014_
	long	LR__0010
ptr_L__0018_
	long	LR__0020
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
