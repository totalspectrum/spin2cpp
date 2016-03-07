DAT
	org	0

count
	mov	count_i_, #0
L_001_
	mov	OUTA, count_i_
	add	count_i_, #1
	cmps	count_i_, #4 wc,wz
 if_b	jmp	#L_001_
count_ret
	ret

arg1_
	long	0
count_i_
	long	0
