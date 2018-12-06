pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_sum2
	rdlong	result1, arg1
	add	arg1, #4
	rdlong	_var01, arg1
	add	result1, _var01
_sum2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg1
	res	1
	fit	496
