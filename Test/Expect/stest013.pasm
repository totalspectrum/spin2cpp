pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_prod
	mov	muldiva_, arg01
	mov	muldivb_, arg02
	call	#unsmultiply_
	mov	result1, muldiva_
_prod_ret
	ret

_cube
	mov	muldiva_, arg01
	mov	muldivb_, arg01
	call	#unsmultiply_
	mov	muldivb_, muldiva_
	mov	muldiva_, arg01
	call	#unsmultiply_
	mov	result1, muldiva_
_cube_ret
	ret

multiply_
       mov    itmp2_, muldiva_
       xor    itmp2_, muldivb_
       abs    muldiva_, muldiva_
       abs    muldivb_, muldivb_
       jmp    #do_multiply_
unsmultiply_
       mov    itmp2_, #0
do_multiply_
	mov    result1, #0
mul_lp_
	shr    muldivb_, #1 wc,wz
 if_c	add    result1, muldiva_
	shl    muldiva_, #1
 if_ne	jmp    #mul_lp_
       shr    itmp2_, #31 wz
       negnz  muldiva_, result1
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
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
