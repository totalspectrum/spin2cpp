pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo1
	rdword	result1, arg01
	add	result1, #2
	wrword	result1, arg01
	rdword	result1, arg01
_foo1_ret
	ret

_foo2
	add	arg01, #4
	rdlong	result1, arg01
_foo2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
