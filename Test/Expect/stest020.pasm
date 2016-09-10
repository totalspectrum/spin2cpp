PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_simplemul
	mov	_var_00, #0
L__0001
	cmps	_var_00, arg1 wc,wz
 if_b	add	_var_00, arg2
 if_b	add	_var_00, #1
 if_b	jmp	#L__0001
	mov	result1, _var_00
_simplemul_ret
	ret

_var_00
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
	fit	496
