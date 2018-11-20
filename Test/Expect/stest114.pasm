pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_init
	mov	_var_02, objptr
	mov	_var_00, #1
LR__0001
	wrlong	_var_00, _var_02
	add	_var_00, #1
	add	_var_02, #4
	cmps	_var_00, #11 wc,wz
 if_b	jmp	#LR__0001
_init_ret
	ret

_initzero
	mov	_var_02, objptr
	mov	_var_00, #10
LR__0002
	mov	_tmp001_, #0
	wrlong	_tmp001_, _var_02
	add	_var_02, #4
	djnz	_var_00, #LR__0002
_initzero_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_tmp001_
	res	1
_var_00
	res	1
_var_02
	res	1
	fit	496
