pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mylongset
	mov	_var01, arg01
	cmp	arg03, #0 wz
 if_e	jmp	#LR__0002
LR__0001
	wrlong	arg02, _var01
	add	_var01, #4
	djnz	arg03, #LR__0001
LR__0002
	mov	result1, arg01
_mylongset_ret
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
arg02
	res	1
arg03
	res	1
	fit	496
