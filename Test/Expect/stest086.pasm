pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_update
	mov	_var01, #0
	add	objptr, #4
	mov	_var02, objptr
	sub	objptr, #4
LR__0001
	cmps	_var01, #10 wc
 if_ae	jmp	#LR__0002
	rdlong	_var03, _var02
	add	_var03, #1
	wrlong	_var03, _var02
	add	_var01, #1
	add	_var02, #4
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
_var01
	res	1
_var02
	res	1
_var03
	res	1
	fit	496
