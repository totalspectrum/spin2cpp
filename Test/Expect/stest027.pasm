DAT
	org	0
hyp
	mov	muldiva_, hyp_x_
	mov	muldivb_, hyp_x_
	call	#multiply_
	mov	hyp_tmp002_, muldiva_
	mov	muldiva_, hyp_y_
	mov	muldivb_, hyp_y_
	call	#multiply_
	add	hyp_tmp002_, muldiva_
	mov	result_, hyp_tmp002_
hyp_ret
	ret

multiply_
	mov	itmp2_, muldiva_
	xor	itmp2_, muldivb_
	abs	muldiva_, muldiva_
	abs	muldivb_, muldivb_
	mov	result_, #0
	mov	itmp1_, #32
	shr	muldiva_, #1 wc
mul_lp_
 if_c	add	result_, muldivb_ wc
	rcr	result_, #1 wc
	rcr	muldiva_, #1 wc
	djnz	itmp1_, #mul_lp_
	shr	itmp2_, #31 wz
 if_nz	neg	result_, result_
 if_nz	neg	muldiva_, muldiva_ wz
 if_nz	sub	result_, #1
	mov	muldivb_, result_
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
muldiva_
	long	0
muldivb_
	long	0
result_
	long	0
