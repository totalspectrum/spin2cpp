PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_foo1
	and	arg1, #255
	mov	result1, arg1
_foo1_ret
	ret

_foo2
	shl	arg1, #24
	sar	arg1, #24
	mov	result1, arg1
_foo2_ret
	ret

_foo3
	shl	arg1, #24
	sar	arg1, #24
	shl	arg1, #16
	shr	arg1, #16
	mov	result1, arg1
_foo3_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
	fit	496
