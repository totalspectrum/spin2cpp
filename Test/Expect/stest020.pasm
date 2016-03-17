DAT
	org	0

_simplemul
	mov	_simplemul_i, #0
	mov	_simplemul_r, #0
L_047_
	cmps	_simplemul_i, arg1 wc,wz
 if_b	add	_simplemul_r, arg2
 if_b	add	_simplemul_i, #1
 if_b	jmp	#L_047_
	mov	result1, _simplemul_r
_simplemul_ret
	ret

_simplemul_i
	long	0
_simplemul_r
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
