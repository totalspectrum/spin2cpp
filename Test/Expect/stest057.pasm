DAT
	org	0

_myfill
L_047_
	cmps	arg3, #0 wz
 if_e	jmp	#L_049_
	wrlong	arg2, arg1
	add	arg1, #4
	sub	arg3, #1
	jmp	#L_047_
L_049_
_myfill_ret
	ret

_fillzero
	mov	arg3, arg2
	mov	arg2, #0
	call	#_myfill
_fillzero_ret
	ret

_fillone
	mov	arg3, arg2
	neg	arg2, #1
	call	#_myfill
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
