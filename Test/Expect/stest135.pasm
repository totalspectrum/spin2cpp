pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zero
	mov	_var01, arg1
	cmps	arg2, #0 wc,wz
 if_be	jmp	#LR__0002
	mov	_var02, arg2
LR__0001
	mov	arg1, #0
	wrword	arg1, _var01
	mov	arg1, _var02
	add	_var01, #2
	djnz	_var02, #LR__0001
LR__0002
_zero_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
