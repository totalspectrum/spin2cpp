DAT
	org	0
ez_pulse_in
	mov	ez_pulse_in_r_, #1
	shl	ez_pulse_in_r_, ez_pulse_in_pin_
	waitpne	ez_pulse_in_r_, ez_pulse_in_r_
	waitpeq	ez_pulse_in_r_, ez_pulse_in_r_
	neg	muldiva_, CNT
	waitpne	ez_pulse_in_r_, ez_pulse_in_r_
	add	muldiva_, CNT
	mov	muldivb_, imm_1000000_
	call	#divide_
	mov	result_, muldiva_
ez_pulse_in_ret
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

ez_pulse_in_pin_
	long	0
ez_pulse_in_r_
	long	0
imm_1000000_
	long	1000000
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
