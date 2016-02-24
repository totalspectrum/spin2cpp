DAT
	org	0
ismagic
	cmps	ismagic_x_, #511 wz
 if_e	mov	ismagic_a_, #1
 if_ne	mov	ismagic_a_, #0
	mov	result_, ismagic_a_
ismagic_ret
	ret

ismagic_a_
	long	0
ismagic_x_
	long	0
result_
	long	0
