pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	_var01, #99
	mov	_var02, arg01 wz
 if_e	jmp	#LR__0001
	cmp	_var02, #1 wz
 if_e	jmp	#LR__0002
	jmp	#LR__0003
LR__0001
LR__0002
	mov	_var01, #2
	jmp	#LR__0004
LR__0003
LR__0004
	mov	result1, _var01
_foo_ret
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
_var02
	res	1
arg01
	res	1
	fit	496
