DAT
	org	0
stest016_count
	mov	stest016_count_i_, #0
L_001_
	mov	OUTA, stest016_count_i_
	add	stest016_count_i_, #1
	jmp	#L_001_
stest016_count_ret
	ret

stest016_count_i_
	long	0
