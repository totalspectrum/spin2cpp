DAT
	org	0
stest028_divmod16
	mov	mula_, stest028_divmod16_x_
	mov	mulb_, stest028_divmod16_y_
	call	#divide_
	mov	stest028_divmod16_q_, mula_
	mov	mula_, stest028_divmod16_x_
	mov	mulb_, stest028_divmod16_y_
	call	#divide_
	shl	stest028_divmod16_q_, #16
	or	stest028_divmod16_q_, mulb_
	mov	result_, stest028_divmod16_q_
stest028_divmod16_ret
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
stest028_divmod16_q_
	long	0
stest028_divmod16_x_
	long	0
stest028_divmod16_y_
	long	0
