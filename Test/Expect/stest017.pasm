DAT
	org	0
stest017_count
	mov	stest017_count_i_, #0
L_001_
	mov	OUTA, stest017_count_i_
	add	stest017_count_i_, #1
	cmps	stest017_count_i_, #4 wz
 if_ne	jmp	#L_001_
stest017_count_ret
	ret

stest017_count_i_
	long	0
