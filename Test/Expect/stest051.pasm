DAT
	org	0

foo
	rdlong	result_, #0
	add	result_, #2
foo_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
