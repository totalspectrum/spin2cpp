PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_demo
	mov	_var_01, #7
L__0002
	mov	arg1, imm_40000000_
	add	arg1, CNT
	waitcnt	arg1, #0
	djnz	_var_01, #L__0002
L__0005
	jmp	#L__0005
_demo_ret
	ret

_Exit
L__0007
	jmp	#L__0007
_Exit_ret
	ret

_PauseABit
	mov	arg1, imm_40000000_
	add	arg1, CNT
	waitcnt	arg1, #0
_PauseABit_ret
	ret

_var_01
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
