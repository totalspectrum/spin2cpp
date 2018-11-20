pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_proc1
	sub	arg2, #1
	mov	_var_02, arg2
	mov	_var_04, #0
	mov	_var_06, arg2
	shl	_var_06, #2
	mov	_tmp002_, arg1
	add	_var_06, _tmp002_
LR__0001
	cmps	_var_02, #0 wc,wz
 if_be	jmp	#LR__0002
	rdlong	_tmp002_, _var_06
	add	_var_04, _tmp002_
	sub	_var_02, #1
	sub	_var_06, #4
	jmp	#LR__0001
LR__0002
	add	_var_04, arg2
	wrlong	_var_04, arg1
_proc1_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp002_
	res	1
_var_02
	res	1
_var_04
	res	1
_var_06
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
