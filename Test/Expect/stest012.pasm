PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_sum
	add	arg1, arg2
	mov	result1, arg1
_sum_ret
	ret

_inc
	add	arg1, #1
	mov	result1, arg1
_inc_ret
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
	fit	496
