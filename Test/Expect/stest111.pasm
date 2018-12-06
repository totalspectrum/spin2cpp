pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_check
	mov	_var01, arg1
	cmp	_var01, #48 wz
 if_e	jmp	#LR__0001
	cmp	_var01, #49 wz
 if_e	jmp	#LR__0001
	cmp	_var01, #50 wz
 if_e	jmp	#LR__0001
	rdlong	_var02, objptr
	cmp	_var01, _var02 wz
 if_ne	jmp	#LR__0002
LR__0001
	mov	result1, #1
	jmp	#_check_ret
LR__0002
	mov	result1, #0
_check_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg1
	res	1
	fit	496
