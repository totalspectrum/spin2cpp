PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

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
       mov    itmp2_, muldiva_
       xor    itmp2_, muldivb_
       abs    muldiva_, muldiva_
       abs    muldivb_, muldivb_
	mov    result1, #0
mul_lp_
	shr    muldivb_, #1 wc,wz
 if_c	add    result1, muldiva_
	shl    muldiva_, #1
 if_ne	jmp    #mul_lp_
       shr    itmp2_, #31 wz
 if_nz neg    result1, result1
	mov    muldiva_, result1
multiply__ret
	ret

itmp1_
	long	0
itmp2_
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
cube_tmp001_
	res	1
muldiva_
	res	1
muldivb_
	res	1
	fit	496
