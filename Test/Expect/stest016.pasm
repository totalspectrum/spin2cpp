DAT
	org	0

count
	mov	count_i_, #0
L_001_
	mov	OUTA, count_i_
	add	count_i_, #1
	jmp	#L_001_
count_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
count_i_
	long	0
