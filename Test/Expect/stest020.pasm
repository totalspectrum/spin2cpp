pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_simplemul
	mov	_var_03, #0
	mov	_var_00, #0
LR__0001
	cmps	_var_03, arg1 wc,wz
 if_b	add	_var_00, arg2
 if_b	add	_var_03, #1
 if_b	jmp	#LR__0001
	mov	result1, _var_00
_simplemul_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_00
	res	1
_var_03
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
