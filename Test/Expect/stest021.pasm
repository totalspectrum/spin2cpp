DAT
	org	0
waitcycles
	mov	waitcycles_end_, CNT
	add	waitcycles_end_, waitcycles_n_
L_001_
	cmps	CNT, waitcycles_end_ wc,wz
 if_lt	jmp	#L_001_
waitcycles_ret
	ret

waitcycles_end_
	long	0
waitcycles_n_
	long	0
