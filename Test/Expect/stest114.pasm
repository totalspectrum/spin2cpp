pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_init
	mov	_var01, objptr
	mov	_var02, #1
LR__0001
	wrlong	_var02, _var01
	add	_var02, #1
	add	_var01, #4
	cmp	_var02, #11 wc
 if_b	jmp	#LR__0001
_init_ret
	ret

_initzero
	mov	_var01, objptr
	mov	_var02, #10
LR__0002
	mov	_var03, #0
	wrlong	_var03, _var01
	add	_var01, #4
	djnz	_var02, #LR__0002
_initzero_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
	fit	496
