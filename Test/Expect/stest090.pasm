PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_proc1
	sub	arg2, #1
	mov	_var_03, arg2
	mov	_var_05, #0
	mov	_var_07, arg2
	shl	_var_07, #2
	add	_var_07, arg1
LR__0001
	cmps	_var_03, #0 wc,wz
 if_be	jmp	#LR__0002
	rdlong	_tmp002_, _var_07
	add	_var_05, _tmp002_
	sub	_var_03, #1
	sub	_var_07, #4
	jmp	#LR__0001
LR__0002
	add	_var_05, arg2
	wrlong	_var_05, arg1
_proc1_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp002_
	res	1
_var_03
	res	1
_var_05
	res	1
_var_07
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
