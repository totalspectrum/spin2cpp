PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_simplemul
	mov	_var_03, #0
	mov	_var_r, #0
L__0003
	cmps	_var_03, arg1 wc,wz
 if_b	add	_var_r, arg2
 if_b	add	_var_03, #1
 if_b	jmp	#L__0003
	mov	result1, _var_r
_simplemul_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_03
	res	1
_var_r
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
