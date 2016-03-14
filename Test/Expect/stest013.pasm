DAT
	org	0

_prod
	mov	muldiva_, arg1
	mov	muldivb_, arg2
	call	#multiply_
	mov	result1, muldiva_
_prod_ret
	ret

_cube
	mov	cube_tmp001_, arg1
	mov	arg2, arg1
	call	#_prod
	mov	arg2, result1
	mov	arg1, cube_tmp001_
	call	#_prod
_cube_ret
	ret

multiply_
	mov	itmp2_, muldiva_
	xor	itmp2_, muldivb_
	abs	muldiva_, muldiva_
	abs	muldivb_, muldivb_
	mov	result1, #0
	mov	itmp1_, #32
	shr	muldiva_, #1 wc
mul_lp_
 if_c	add	result1, muldivb_ wc
	rcr	result1, #1 wc
	rcr	muldiva_, #1 wc
	djnz	itmp1_, #mul_lp_
	shr	itmp2_, #31 wz
 if_nz	neg	result1, result1
 if_nz	neg	muldiva_, muldiva_ wz
 if_nz	sub	result1, #1
	mov	muldivb_, result1
multiply__ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
cube_tmp001_
	long	0
itmp1_
	long	0
itmp2_
	long	0
muldiva_
	long	0
muldivb_
	long	0
result1
	long	0
	fit	496
