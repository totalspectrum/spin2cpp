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
	mov	result1, muldivb_
	mov	muldiva_, arg02
	mov	muldivb_, arg03
	call	#unsmultiply_
	add	result1, muldiva_
	mov	muldiva_, arg04
	mov	muldivb_, arg01
	call	#unsmultiply_
	add	result1, muldiva_
_mul_hi_ret
	ret

unsmultiply_
       jmpret $, #do_multiply_ nr,wc

multiply_
       abs    muldiva_, muldiva_ wc
       negnc  itmp1_,#1
       abs    muldivb_, muldivb_ wc
       muxnc  itmp1_,#1 wc
do_multiply_
       rcr    muldiva_, #1 wc
       mov    itmp2_, #0
       mov    itmp1_, #32
mul_lp_
 if_c  add    itmp2_, muldivb_ wc
       rcr    itmp2_, #1 wc
       rcr    muldiva_, #1 wc
       djnz    itmp1_, #mul_lp_
       negc   muldivb_, itmp2_
 if_c  neg    muldiva_, muldiva_ wz
 if_c_and_nz sub    muldivb_, #1
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
arg01
	res	1
arg02
	res	1
arg03
	res	1
arg04
	res	1
	fit	496
