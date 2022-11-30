pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get_high
	mov	_var01, arg01
	mov	_var02, arg02
	mov	result1, _var02
_get_high_ret
	ret

_get_low
	mov	result2, arg01
	mov	result1, #0
_get_low_ret
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
arg01
	res	1
arg02
	res	1
	fit	496
