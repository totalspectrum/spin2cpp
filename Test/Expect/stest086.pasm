pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_update
	mov	_var01, #0
	mov	_var02, objptr
LR__0001
	add	_var02, #4
	cmps	_var01, #10 wc
 if_b	rdlong	_var03, _var02
 if_b	add	_var03, #1
 if_b	wrlong	_var03, _var02
 if_b	add	_var01, #1
 if_b	jmp	#LR__0001
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
