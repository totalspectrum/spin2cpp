DAT
	org	0

count1
	mov	count1_i_, #5
L_001_
	xor	OUTA, #2
	djnz	count1_i_, #L_001_
count1_ret
	ret

count1_i_
	long	0
