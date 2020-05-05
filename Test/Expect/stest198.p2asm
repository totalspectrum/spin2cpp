pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setval2
	mov	_var01, arg01
	andn	_var01, #3
	or	_var01, #1
	mov	result1, _var01
_setval2_ret
	ret

_setval1
	mov	_var01, arg01
	andn	_var01, #3
	or	_var01, #1
	mov	result1, _var01
_setval1_ret
	ret

_getval2
	sar	arg01, #4
	and	arg01, #255
	mov	result1, arg01
_getval2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
