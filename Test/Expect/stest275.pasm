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
	rdlong	_var05, arg01
	rdlong	result1, _var05
	add	_var05, #4
	rdlong	_var02, _var05
	rdlong	_var06, arg01
	add	_var06, #8
	wrlong	_var06, arg01
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
_var06
	res	1
arg01
	res	1
	fit	496
