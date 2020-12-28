pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test1
	add	arg01, arg01
	add	arg02, arg02
	xor	arg01, arg02
	mov	result1, arg01
_test1_ret
	ret

_test2
	add	arg01, arg01
	add	arg02, arg02
	xor	arg01, arg02
	mov	result1, arg01
_test2_ret
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
arg02
	res	1
	fit	496
