DAT
	org	0

_waitcycles
	mov	_waitcycles_end, CNT
	add	_waitcycles_end, arg1_
L_032_
	cmps	CNT, _waitcycles_end wc,wz
 if_b	jmp	#L_032_
_waitcycles_ret
	ret

_waitcycles_end
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
