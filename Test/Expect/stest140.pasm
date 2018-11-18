PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_factorial
	wrlong	local01, sp
	add	sp, #4
	wrlong	local02, sp
	add	sp, #4
	wrlong	_factorial_ret, sp
	add	sp, #4
_factorial_enter
	mov	local01, arg2
	cmps	arg1, #0 wc,wz
 if_be	jmp	#LR__0001
	mov	local02, arg1
	sub	local02, #1
	mov	muldiva_, local01
	mov	muldivb_, arg1
	call	#multiply_
	mov	arg1, local02
	mov	arg2, muldiva_
	jmp	#_factorial_enter
LR__0001
	mov	result1, local01
	sub	sp, #4
	rdlong	_factorial_ret, sp
	sub	sp, #4
	rdlong	local02, sp
	sub	sp, #4
	rdlong	local01, sp
_factorial_ret
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
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
stackspace
	long	0[1]
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
local01
	res	1
local02
	res	1
muldiva_
	res	1
muldivb_
	res	1
	fit	496
