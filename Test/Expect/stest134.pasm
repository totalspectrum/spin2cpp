pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo1
	and	arg01, #255
	mov	result1, arg01
_foo1_ret
	ret

_foo2
	shl	arg01, #24
	sar	arg01, #24
	mov	result1, arg01
_foo2_ret
	ret

_foo3
	shl	arg01, #24
	sar	arg01, #24
	shl	arg01, #16
	shr	arg01, #16
	mov	result1, arg01
_foo3_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
