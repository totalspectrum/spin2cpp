PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_times1
	mov	result1, arg1
_times1_ret
	ret

_times3
	mov	result1, arg1
	shl	result1, #1
	add	result1, arg1
_times3_ret
	ret

_times4
	shl	arg1, #2
	mov	result1, arg1
_times4_ret
	ret

_times5
	mov	result1, arg1
	shl	result1, #2
	add	result1, arg1
_times5_ret
	ret

_times6
	mov	result1, arg1
	shl	result1, #1
	add	result1, arg1
	shl	result1, #1
_times6_ret
	ret

_times7
	mov	result1, arg1
	shl	result1, #3
	sub	result1, arg1
_times7_ret
	ret

_times9
	mov	result1, arg1
	shl	result1, #3
	add	result1, arg1
_times9_ret
	ret

_times10
	mov	result1, arg1
	shl	result1, #2
	add	result1, arg1
	shl	result1, #1
_times10_ret
	ret

_times11
	mov	muldiva_, arg1
	mov	muldivb_, #11
	call	#multiply_
	mov	result1, muldiva_
_times11_ret
	ret

_times12
	mov	result1, arg1
	shl	result1, #1
	add	result1, arg1
	shl	result1, #2
_times12_ret
	ret

_times15
	mov	result1, arg1
	shl	result1, #4
	sub	result1, arg1
_times15_ret
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
