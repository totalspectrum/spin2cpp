DAT
	org	0

waitcycles
	mov	waitcycles_end_, CNT
	add	waitcycles_end_, arg1_
L_001_
	cmps	CNT, waitcycles_end_ wc,wz
 if_b	jmp	#L_001_
waitcycles_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
waitcycles_end_
	long	0
