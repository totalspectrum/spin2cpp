PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test1
	mov	test1_tmp001_, #0
	cmps	arg1, arg2 wc,wz
 if_b	neg	test1_tmp001_, #1
	mov	result1, test1_tmp001_
_test1_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
test1_tmp001_
	long	0
	fit	496
