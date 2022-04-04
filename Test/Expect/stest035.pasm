pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_sum2
	rdlong	result1, arg01
	add	arg01, #4
	rdlong	arg01, arg01
	add	result1, arg01
_sum2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
