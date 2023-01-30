pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mul_hi
	mov	arg04, #0
	mov	arg03, arg02
	mov	arg02, #0
	mov	muldiva_, arg01
	mov	muldivb_, arg03
	call	#unsmultiply_
	mov	_var01, muldivb_
	mov	muldiva_, arg02
	mov	muldivb_, arg03
	call	#unsmultiply_
	add	_var01, muldiva_
	mov	muldiva_, arg04
	mov	muldivb_, arg01
	call	#unsmultiply_
	add	_var01, muldiva_
	mov	result1, _var01
_mul_hi_ret
	ret

multiply_
	mov	itmp2_, muldiva_
	xor	itmp2_, muldivb_
	abs	muldiva_, muldiva_
	abs	muldivb_, muldivb_
	jmp	#do_multiply_

unsmultiply_
	mov	itmp2_, #0
do_multiply_
	mov	result1, #0
	mov	itmp1_, #32
	shr	muldiva_, #1 wc
mul_lp_
 if_c	add	result1, muldivb_ wc
	rcr	result1, #1 wc
	rcr	muldiva_, #1 wc
	djnz	itmp1_, #mul_lp_
	shr	itmp2_, #31 wz
	negnz	muldivb_, result1
 if_nz	neg	muldiva_, muldiva_ wz
 if_nz	sub	muldivb_, #1
multiply__ret
unsmultiply__ret
	ret

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
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
arg04
	res	1
	fit	496
