pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getfloat
	add	arg01, #4
	rdlong	result1, arg01
_getfloat_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
