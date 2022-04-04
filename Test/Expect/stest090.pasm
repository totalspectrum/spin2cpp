pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_proc1
	sub	arg02, #1
	mov	_var01, arg02
	mov	_var02, #0
	mov	_var03, arg02
	shl	_var03, #2
	add	_var03, arg01
LR__0001
	cmps	_var01, #1 wc
 if_ae	rdlong	_var04, _var03
 if_ae	add	_var02, _var04
 if_ae	sub	_var01, #1
 if_ae	sub	_var03, #4
 if_ae	jmp	#LR__0001
	add	_var02, arg02
	wrlong	_var02, arg01
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
arg01
	res	1
arg02
	res	1
	fit	496
