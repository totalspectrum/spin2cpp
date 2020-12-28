pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_doit
	mov	_var01, arg01
LR__0001
	cmp	_var01, arg01 wc,wz
 if_a	jmp	#LR__0002
	mov	_var02, #1
	shl	_var02, _var01
	or	outa, _var02
	sub	_var01, #1
	jmp	#LR__0001
LR__0002
_doit_ret
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
	fit	496
