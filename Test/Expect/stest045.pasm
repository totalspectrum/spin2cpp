DAT
	org	0

_strlen2
	mov	_strlen2_r, #0
L_016_
	rdbyte	strlen2_tmp001_, arg1_ wz
 if_ne	add	arg1_, #1
 if_ne	add	_strlen2_r, #1
 if_ne	jmp	#L_016_
	mov	result_, _strlen2_r
_strlen2_ret
	ret

_strlen2_r
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
strlen2_tmp001_
	long	0
