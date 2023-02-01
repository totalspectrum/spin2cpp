pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_hyp
	mov	muldiva_, arg01
	mov	muldivb_, arg01
	call	#unsmultiply_
	mov	result1, muldiva_
	mov	muldiva_, arg02
	mov	muldivb_, arg02
	call	#unsmultiply_
	add	result1, muldiva_
_hyp_ret
	ret

unsmultiply_
multiply_
       mov    itmp1_, #0
mul_lp_
       shr    muldivb_, #1 wc,wz
 if_c  add    itmp1_, muldiva_
       shl    muldiva_, #1
 if_ne jmp    #mul_lp_
       mov    muldiva_, itmp1_
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
