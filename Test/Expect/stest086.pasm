PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_update
	mov	_var_01, #0
	add	objptr, #4
	mov	_var_02, objptr
	sub	objptr, #4
L__0003
	cmps	_var_01, #10 wc,wz
 if_ae	jmp	#L__0004
	rdlong	_var_03, _var_02
	add	_var_03, #1
	wrlong	_var_03, _var_02
	add	_var_01, #1
	add	_var_02, #4
	jmp	#L__0003
L__0004
_update_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	11
	org	COG_BSS_START
_var_01
	res	1
_var_02
	res	1
_var_03
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
