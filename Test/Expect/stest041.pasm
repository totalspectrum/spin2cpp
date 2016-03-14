DAT
	org	0

_count1
	mov	_count1_i, #5
L_032_
	xor	OUTA, #2
	djnz	_count1_i, #L_032_
_count1_ret
	ret

_count1_i
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
	fit	496
