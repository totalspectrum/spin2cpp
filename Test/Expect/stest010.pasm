pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test1
	add	arg1, arg1
	add	arg2, arg2
	xor	arg1, arg2
	mov	result1, arg1
_test1_ret
	ret

_test2
	add	arg1, arg1
	add	arg2, arg2
	xor	arg1, arg2
	mov	result1, arg1
_test2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
	fit	496
