DAT
	org	0
stest019_count
	mov	stest019_count_i_, #0
L_001_
	mov	OUTA, stest019_count_i_
	add	stest019_count_i_, #1
	cmps	stest019_count_i_, #4 wc,wz
 if_lt	jmp	#L_001_
stest019_count_ret
	ret

stest019_count_i_
	long	0
