pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getit9
_getit3
_getit2
_getit1
	rdlong	result1, arg01
	add	arg01, #4
	rdlong	result2, arg01
_getit1_ret
_getit9_ret
_getit3_ret
_getit2_ret
	ret




_getitA
	rdlong	result2, arg01
	rdlong	result1, result2
	add	result2, #4
	rdlong	result2, result2
	rdlong	_var01, arg01
	add	_var01, #8
	wrlong	_var01, arg01
_getitA_ret
	ret

result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
