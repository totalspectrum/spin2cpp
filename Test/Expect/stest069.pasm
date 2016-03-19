DAT
	org	0

_demo
	mov	_demo__idx__0016, #7
L_047_
	mov	arg1, imm_40000000_
	add	arg1, CNT
	waitcnt	arg1, #0
	djnz	_demo__idx__0016, #L_047_
L_050_
	jmp	#L_050_
_demo_ret
	ret

_Exit
L_052_
	jmp	#L_052_
_Exit_ret
	ret

_PauseABit
	mov	arg1, imm_40000000_
	add	arg1, CNT
	waitcnt	arg1, #0
_PauseABit_ret
	ret

_demo__idx__0016
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
imm_40000000_
	long	40000000
result1
	long	0
	fit	496
