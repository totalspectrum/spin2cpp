pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_checkit
	mov	_var01, #0
LR__0001
	mov	_var02, ina
	and	_var02, #255
	mov	_var03, _var02
	cmp	_var03, #121 wz
 if_e	jmp	#LR__0002
	cmp	_var03, #89 wz
 if_e	jmp	#LR__0002
	mov	_var04, _var01
	mov	_var05, _var01
	add	_var05, #1
	mov	_var01, _var05
	jmp	#LR__0001
LR__0002
	mov	muldiva_, _var01
	mov	muldivb_, #27
	call	#multiply_
	mov	result1, muldiva_
_checkit_ret
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
muldiva_
	long	0
muldivb_
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
_var05
	res	1
	fit	496
