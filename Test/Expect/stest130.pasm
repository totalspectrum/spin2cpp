pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	add	arg1, arg2
	mov	result1, arg1
_foo_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
	fit	496
