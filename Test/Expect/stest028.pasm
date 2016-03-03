DAT
	org	0
divmod16
	mov	divmod16_x_, arg1_
	mov	divmod16_y_, arg2_
	mov	muldiva_, divmod16_x_
	mov	muldivb_, divmod16_y_
	call	#divide_
	mov	divmod16_q_, muldiva_
	mov	muldiva_, divmod16_x_
	mov	muldivb_, divmod16_y_
	call	#divide_
	shl	divmod16_q_, #16
	or	divmod16_q_, muldivb_
	mov	result_, divmod16_q_
divmod16_ret
	ret

divide_
	mov	result_, #0
	mov	itmp2_, muldiva_
	xor	itmp2_, muldivb_
	abs	muldiva_, muldiva_
	abs	muldivb_, muldivb_ wz,wc
 if_z	jmp	#divexit_
	muxc	itmp2_, #1
	mov	itmp1_, #32
divlp1_
	shr	muldivb_,#1 wc,wz
	rcr	result_,#1
 if_nz	djnz	itmp1_,#divlp1_
divlp2_
	cmpsub	muldiva_,result_ wc
	rcl	muldivb_,#1
	shr	result_,#1
	djnz	itmp1_,#divlp2_
	cmps	itmp2_, #0 wc,wz
 if_b	neg	muldiva_, muldiva_
	test	itmp2, #1
 if_nz	neg	muldivb_, muldivb_
divide__ret
	ret

arg1_
	long	0
arg2_
	long	0
divmod16_q_
	long	0
divmod16_x_
	long	0
divmod16_y_
	long	0
itmp1_
	long	0
itmp2_
	long	0
muldiva_
	long	0
muldivb_
	long	0
result_
	long	0
