DAT
	org	0

_foo
	rdlong	result1, #0
	add	result1, #2
_foo_ret
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
