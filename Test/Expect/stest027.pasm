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
	mov	_var_03, muldiva_
	mov	muldiva_, _var_02
	mov	muldivb_, _var_02
	call	#multiply_
	add	_var_03, muldiva_
	mov	result1, _var_03
_hyp_ret
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
_var_02
	res	1
_var_03
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
muldiva_
	res	1
muldivb_
	res	1
	fit	496
