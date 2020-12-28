pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_check
	cmp	arg01, imm_4294967295_ wz
 if_e	jmp	#LR__0001
	cmp	arg01, #0 wz
 if_e	jmp	#LR__0002
	jmp	#LR__0003
LR__0001
	mov	result1, #0
	jmp	#_check_ret
LR__0002
	mov	result1, #1
	jmp	#_check_ret
LR__0003
	neg	result1, #1
_check_ret
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
	fit	496
