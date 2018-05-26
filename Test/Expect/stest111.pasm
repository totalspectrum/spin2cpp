PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_check
	mov	_var_01, arg1
	cmps	_var_01, #48 wz
 if_e	jmp	#L__0003
	cmps	_var_01, #49 wz
 if_e	jmp	#L__0003
	cmps	_var_01, #50 wz
 if_e	jmp	#L__0003
	rdlong	_tmp001_, objptr
	cmps	_var_01, _tmp001_ wz
 if_ne	jmp	#L__0002
L__0003
	mov	result1, #1
	jmp	#_check_ret
L__0002
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
_tmp001_
	res	1
_var_01
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
