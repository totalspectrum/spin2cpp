DAT
	org	0
hyp
	mov	mula_, hyp_x_
	mov	mulb_, hyp_x_
	call	#multiply_
	mov	hyp_tmp002_, mula_
	mov	mula_, hyp_y_
	mov	mulb_, hyp_y_
	call	#multiply_
	add	hyp_tmp002_, mula_
	mov	result_, hyp_tmp002_
hyp_ret
	ret

multiply_
	mov	itmp2_, mula_
	xor	itmp2_, mulb_
	abs	mula_, mula_
	abs	mulb_, mulb_
	mov	result_, #0
	mov	itmp1_, #32
	shr	mula_, #1 wc
mul_lp_
 if_c	add	result_, mulb_ wc
	rcr	result_, #1 wc
	rcr	mula_, #1 wc
	djnz	itmp1_, #mul_lp_
	shr	itmp2_, #31 wz
 if_nz	neg	result_, result_
 if_nz	neg	mula_, mula_ wz
 if_nz	sub	result_, #1
	mov	mulb_, result_
multiply__ret
	ret

hyp_tmp002_
	long	0
hyp_x_
	long	0
hyp_y_
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
