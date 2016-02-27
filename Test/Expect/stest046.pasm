DAT
	org	0
ez_pulse_in
	mov	ez_pulse_in_r_, #1
	shl	ez_pulse_in_r_, ez_pulse_in_pin_
	waitpne	ez_pulse_in_r_, ez_pulse_in_r_
	waitpeq	ez_pulse_in_r_, ez_pulse_in_r_
	neg	mula_, CNT
	waitpne	ez_pulse_in_r_, ez_pulse_in_r_
	add	mula_, CNT
	mov	mulb_, imm_1000000_
	call	#divide_
	mov	result_, mula_
ez_pulse_in_ret
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
mula_
	long	0
mulb_
	long	0
result_
	long	0
