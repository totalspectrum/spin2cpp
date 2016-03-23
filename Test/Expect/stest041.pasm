DAT
	org	0

_count1
	mov	_count1_i, #5
L__0001
	xor	OUTA, #2
	djnz	_count1_i, #L__0001
_count1_ret
	ret

_count1_i
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
