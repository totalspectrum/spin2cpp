pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_stepif
	rdlong	_var01, arg01 wz
 if_ne	add	arg01, #4
 if_ne	rdlong	arg01, arg01
	mov	result1, arg01
_stepif_ret
	ret

_storeif
	rdlong	_var01, arg01 wz
 if_e	jmp	#LR__0001
	mov	_var01, arg01
	add	arg01, #8
	wrlong	_var01, arg01
	sub	arg01, #8
LR__0001
	mov	result1, arg01
_storeif_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
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
