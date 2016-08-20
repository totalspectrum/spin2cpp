PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_sum2
	add	arg1, #8
	rdlong	result1, arg1
	add	arg1, #4
	rdlong	_tmp002_, arg1
	add	result1, _tmp002_
_sum2_ret
	ret

_tmp002_
	long	0
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
	fit	496
