DAT
	org	0

_simplemul
	mov	_simplemul_i, #0
	mov	_simplemul_r, #0
L_001_
	cmps	_simplemul_i, arg1_ wc,wz
 if_b	add	_simplemul_r, arg2_
 if_b	add	_simplemul_i, #1
 if_b	jmp	#L_001_
	mov	result_, _simplemul_r
_simplemul_ret
	ret

_simplemul_i
	long	0
_simplemul_r
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
