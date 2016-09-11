PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_hyp
	mov	_var_02, arg2
	mov	muldiva_, arg1
	mov	muldivb_, arg1
	call	#multiply_
	mov	_tmp002_, muldiva_
	mov	muldiva_, _var_02
	mov	muldivb_, _var_02
	call	#multiply_
	add	_tmp002_, muldiva_
	mov	result1, _tmp002_
_hyp_ret
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

_tmp002_
	long	0
_var_02
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
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
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
