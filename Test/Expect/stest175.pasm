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
	mov	result1, arg02
	add	result1, arg03
	jmp	#_calcresult_ret
LR__0002
	mov	result1, arg02
	sub	result1, arg03
	jmp	#_calcresult_ret
LR__0003
	mov	result1, arg02
_calcresult_ret
	ret

__lockreg
	long	0
imm_4294967295_
	long	-1
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
