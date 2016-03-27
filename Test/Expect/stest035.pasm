PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_sum2
	rdlong	result1, arg1
	add	arg1, #4
	rdlong	sum2_tmp002_, arg1
	add	result1, sum2_tmp002_
_sum2_ret
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
sum2_tmp002_
	long	0
	fit	496
