DAT
	org	0

_count
	mov	_count_i, #0
L_039_
	mov	OUTA, _count_i
	add	_count_i, #1
	cmps	_count_i, #4 wc,wz
 if_b	jmp	#L_039_
_count_ret
	ret

_count_i
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
