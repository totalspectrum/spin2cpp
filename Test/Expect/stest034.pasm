PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_indsum
	rdlong	result1, arg1
	rdlong	_tmp002_, arg2
	add	result1, _tmp002_
_indsum_ret
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
