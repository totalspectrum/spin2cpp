DAT
	org	0
stest022_ismagic
	cmp	stest022_ismagic_x_, #511 wc,wz
 if_eq	mov	stest022_ismagic_a_, #1
 if_ne	mov	stest022_ismagic_a_, #0
	mov	result_, stest022_ismagic_a_
stest022_ismagic_ret
	ret

result_
	long	0
stest022_ismagic_a_
	long	0
stest022_ismagic_x_
	long	0
