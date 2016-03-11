DAT
	org	0

_count
	mov	_count_i, #0
L_032_
	mov	OUTA, _count_i
	add	_count_i, #1
	cmps	_count_i, #4 wz
 if_ne	jmp	#L_032_
_count_ret
	ret

_count_i
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
