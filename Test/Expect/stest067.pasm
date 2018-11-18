PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fibo
	wrlong	local01, sp
	add	sp, #4
	wrlong	local02, sp
	add	sp, #4
	wrlong	local03, sp
	add	sp, #4
	wrlong	_fibo_ret, sp
	add	sp, #4
_fibo_enter
	mov	local01, arg1
	cmps	local01, #2 wc,wz
 if_b	mov	result1, local01
 if_b	jmp	#LR__0001
	mov	arg1, local01
	sub	arg1, #1
	mov	local02, local01
	sub	local02, #2
	call	#_fibo
	mov	local03, result1
	mov	arg1, local02
	call	#_fibo
	add	result1, local03
LR__0001
	sub	sp, #4
	rdlong	_fibo_ret, sp
	sub	sp, #4
	rdlong	local03, sp
	sub	sp, #4
	rdlong	local02, sp
	sub	sp, #4
	rdlong	local01, sp
_fibo_ret
	ret

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
local01
	res	1
local02
	res	1
local03
	res	1
	fit	496
