pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getit1
	rdlong	result1, arg01
	add	arg01, #4
	rdlong	result2, arg01
_getit1_ret
	ret

_getit2
	rdlong	result1, arg01
	add	arg01, #4
	rdlong	result2, arg01
_getit2_ret
	ret

_getit3
	rdlong	result1, arg01
	add	arg01, #4
	rdlong	result2, arg01
_getit3_ret
	ret

_getit9
	rdlong	result1, arg01
	add	arg01, #4
	rdlong	result2, arg01
_getit9_ret
	ret

_getitA
	rdlong	result2, arg01
	rdlong	result1, result2
	add	result2, #4
	rdlong	_var02, result2
	rdlong	_var05, arg01
	add	_var05, #8
	wrlong	_var05, arg01
	mov	_var05, _var02
	mov	_var04, _var05
	mov	result2, _var04
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
_var02
	res	1
_var03
	res	1
_var04
	res	1
_var05
	res	1
arg01
	res	1
	fit	496
