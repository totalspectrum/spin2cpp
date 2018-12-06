pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fibolp
	mov	_var01, #1
	mov	_var02, #1
	mov	_var03, #0
	sub	arg1, #1 wz
 if_e	jmp	#LR__0002
LR__0001
	add	_var03, _var01
	mov	_var04, _var03
	mov	_var03, _var01
	mov	_var01, _var02
	mov	_var02, _var04
	djnz	arg1, #LR__0001
LR__0002
	mov	result1, _var02
_fibolp_ret
	ret

result1
	long	0
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
	fit	496
