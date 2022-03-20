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
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
_fibo_enter
	mov	local01, arg01
	cmps	local01, #2 wc
 if_b	mov	result1, local01
 if_b	jmp	#LR__0001
	mov	arg01, local01
	sub	arg01, #1
	sub	local01, #2
	call	#_fibo
	mov	local02, result1
	mov	arg01, local01
	call	#_fibo
	add	result1, local02
LR__0001
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
	sub	sp, #4
	rdlong	local02, sp
	sub	sp, #4
	rdlong	local01, sp
	sub	sp, #4
	rdlong	_fibo_ret, sp
	nop
_fibo_ret
	ret

fp
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
local01
	res	1
local02
	res	1
	fit	496
