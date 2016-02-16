stest021_waitcycles
	mov	stest021_waitcycles_end_, CNT
	add	stest021_waitcycles_end_, stest021_waitcycles_n_
L_001_
	cmp	CNT, stest021_waitcycles_end_ wc,wz
 if_lt	jmp	#L_001_
stest021_waitcycles_ret
	ret

stest021_waitcycles_end_
	long	0
stest021_waitcycles_n_
	long	0
