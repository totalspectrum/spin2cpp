PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test1
	mov	_tmp001_, #0
	cmps	arg1, arg2 wc,wz
 if_b	neg	_tmp001_, #1
	mov	result1, _tmp001_
_test1_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
