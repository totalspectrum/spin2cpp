DAT
	org	0

_times1
	mov	result_, arg1_
_times1_ret
	ret

_times3
	mov	result_, arg1_
	shl	result_, #1
	add	result_, arg1_
_times3_ret
	ret

_times4
	shl	arg1_, #2
	mov	result_, arg1_
_times4_ret
	ret

_times5
	mov	result_, arg1_
	shl	result_, #2
	add	result_, arg1_
_times5_ret
	ret

_times6
	mov	result_, arg1_
	shl	result_, #1
	add	result_, arg1_
	shl	result_, #1
_times6_ret
	ret

_times7
	mov	result_, arg1_
	shl	result_, #3
	sub	result_, arg1_
_times7_ret
	ret

_times9
	mov	result_, arg1_
	shl	result_, #3
	add	result_, arg1_
_times9_ret
	ret

_times10
	mov	result_, arg1_
	shl	result_, #2
	add	result_, arg1_
	shl	result_, #1
_times10_ret
	ret

_times11
	mov	muldiva_, arg1_
	mov	muldivb_, #11
	call	#multiply_
	mov	result_, muldiva_
_times11_ret
	ret

_times12
	mov	result_, arg1_
	shl	result_, #1
	add	result_, arg1_
	shl	result_, #2
_times12_ret
	ret

_times15
	mov	result_, arg1_
	shl	result_, #4
	sub	result_, arg1_
_times15_ret
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

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
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
