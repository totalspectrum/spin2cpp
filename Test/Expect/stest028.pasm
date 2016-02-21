DAT
	org	0
divmod16
	mov	mula_, divmod16_x_
	mov	mulb_, divmod16_y_
	call	#divide_
	mov	divmod16_q_, mula_
	mov	mula_, divmod16_x_
	mov	mulb_, divmod16_y_
	call	#divide_
	shl	divmod16_q_, #16
	or	divmod16_q_, mulb_
	mov	result_, divmod16_q_
divmod16_ret
	ret

divide_
	mov	result_, #0
	mov	itmp2_, mula_
	xor	itmp2_, mulb_
	abs	mula_, mula_
	abs	mulb_, mulb_ wz,wc
 if_z	jmp	#divexit_
	muxc	itmp2_, #1
	mov	itmp1_, #32
divlp1_
	shr	mulb_,#1 wc,wz
	rcr	result_,#1
 if_nz	djnz	itmp1_,#divlp1_
divlp2_
	cmpsub	mula_,result_ wc
	rcl	mulb_,#1
	shr	result_,#1
	djnz	itmp1_,#divlp2_
	cmps	itmp2_, #0 wc,wz
 if_b	neg	mula_, mula_
	test	itmp2, #1
 if_nz	neg	mulb_, mulb_
divide__ret
	ret

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
mula_
	long	0
mulb_
	long	0
result_
	long	0
