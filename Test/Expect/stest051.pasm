DAT
	org	0

_foo
	rdlong	result_, #0
	add	result_, #2
_foo_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
