pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	_var01, #99
	cmp	arg01, #0 wz
 if_e	jmp	#LR__0001
	cmp	arg01, #1 wz
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
