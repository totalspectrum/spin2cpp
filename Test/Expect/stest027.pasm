DAT
	org	0

_hyp
	mov	_hyp_y, arg2_
	mov	muldiva_, arg1_
	mov	muldivb_, arg1_
	call	#multiply_
	mov	hyp_tmp002_, muldiva_
	mov	muldiva_, _hyp_y
	mov	muldivb_, _hyp_y
	call	#multiply_
	add	hyp_tmp002_, muldiva_
	mov	result_, hyp_tmp002_
_hyp_ret
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

_hyp_y
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
hyp_tmp002_
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
	fit	496
