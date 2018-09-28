PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_update
	mov	_var_00, #0
	add	objptr, #4
	mov	_var_01, objptr
	sub	objptr, #4
LR__0001
	cmps	_var_00, #10 wc,wz
 if_ae	jmp	#LR__0002
	rdlong	_var_02, _var_01
	add	_var_02, #1
	wrlong	_var_02, _var_01
	add	_var_00, #1
	add	_var_01, #4
	jmp	#LR__0001
LR__0002
_update_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[11]
	org	COG_BSS_START
_var_00
	res	1
_var_01
	res	1
_var_02
	res	1
	fit	496
