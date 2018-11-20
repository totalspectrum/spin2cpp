pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zeroit
	mov	_var_06, arg1
	mov	_var_03, #1
	add	arg2, #1
LR__0001
	cmps	_var_03, arg2 wc,wz
 if_ae	jmp	#LR__0002
	mov	arg1, #0
	wrbyte	arg1, _var_06
	add	_var_03, #1
	add	_var_06, #1
	jmp	#LR__0001
LR__0002
	mov	result1, #0
_zeroit_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_03
	res	1
_var_06
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
