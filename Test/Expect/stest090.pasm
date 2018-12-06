pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_proc1
	sub	arg2, #1
	mov	_var01, arg2
	mov	_var02, #0
	mov	_var03, arg2
	shl	_var03, #2
	mov	_var04, arg1
	add	_var03, _var04
LR__0001
	cmps	_var01, #0 wc,wz
 if_be	jmp	#LR__0002
	rdlong	_var04, _var03
	add	_var02, _var04
	sub	_var01, #1
	sub	_var03, #4
	jmp	#LR__0001
LR__0002
	add	_var02, arg2
	wrlong	_var02, arg1
_proc1_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
