pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_factorial
	wrlong	_factorial_ret, sp
	add	sp, #4
	wrlong	local01, sp
	add	sp, #4
	wrlong	local02, sp
	add	sp, #4
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
_factorial_enter
	mov	local01, arg02
	cmps	arg01, #1 wc
 if_b	jmp	#LR__0001
	mov	local02, arg01
	sub	local02, #1
	mov	muldiva_, local01
	mov	muldivb_, arg01
	call	#unsmultiply_
	mov	arg02, muldiva_
	mov	arg01, local02
	jmp	#_factorial_enter
LR__0001
	mov	result1, local01
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
	sub	sp, #4
	rdlong	local02, sp
	sub	sp, #4
	rdlong	local01, sp
	sub	sp, #4
	rdlong	_factorial_ret, sp
	nop
_factorial_ret
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

fp
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
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
stackspace
	long	0[1]
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
local01
	res	1
local02
	res	1
	fit	496
