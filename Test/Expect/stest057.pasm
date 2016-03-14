DAT
	org	0

_myfill
L_043_
	wrlong	arg2, arg1
	add	arg1, #4
	djnz	arg3, #L_043_
_myfill_ret
	ret

_fillzero
	mov	arg3, arg2
	mov	arg2, #0
L_039_
	wrlong	arg2, arg1
	add	arg1, #4
	djnz	arg3, #L_039_
_fillzero_ret
	ret

_fillone
	mov	arg3, arg2
	neg	arg2, #1
L_042_
	wrlong	arg2, arg1
	add	arg1, #4
	djnz	arg3, #L_042_
_fillone_ret
	ret

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
