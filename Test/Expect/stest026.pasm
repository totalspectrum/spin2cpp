pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test1
	mov	_var01, #0
	cmps	arg01, arg02 wc
 if_b	neg	_var01, #1
	mov	result1, _var01
_test1_ret
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
arg02
	res	1
	fit	496
