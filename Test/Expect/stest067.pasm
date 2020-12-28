pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fibo
	wrlong	_fibo_ret, sp
	add	sp, #4
	wrlong	local01, sp
	add	sp, #4
	wrlong	local02, sp
	add	sp, #4
	wrlong	local03, sp
	add	sp, #4
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
_fibo_enter
	cmps	arg01, #2 wc,wz
 if_b	mov	result1, arg01
 if_b	jmp	#LR__0001
	mov	local01, arg01
	sub	local01, #1
	mov	local02, arg01
	sub	local02, #2
	mov	arg01, local01
	call	#_fibo
	mov	local03, result1
	mov	arg01, local02
	call	#_fibo
	add	result1, local03
LR__0001
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
	sub	sp, #4
	rdlong	local03, sp
	sub	sp, #4
	rdlong	local02, sp
	sub	sp, #4
	rdlong	local01, sp
	sub	sp, #4
	rdlong	_fibo_ret, sp
	nop
_fibo_ret
	ret

__lockreg
	long	0
fp
	long	0
ptr___lockreg_
	long	@@@__lockreg
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
local01
	res	1
local02
	res	1
local03
	res	1
	fit	496
