DAT
	org	0

foo
	rdlong	result_, #0
	add	result_, #2
foo_ret
	ret

result_
	long	0
