pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zero
	mov	_var_03, arg1
	cmps	arg2, #0 wc,wz
 if_be	jmp	#LR__0002
	mov	_var_02, arg2
LR__0001
	mov	arg1, #0
	wrword	arg1, _var_03
	mov	arg1, _var_02
	add	_var_03, #2
	djnz	_var_02, #LR__0001
LR__0002
_zero_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_02
	res	1
_var_03
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
