pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	result1, ina
	shr	result1, #4
	and	result1, #1
_main_ret
	ret

_simplepin_getval
	mov	result1, ina
	shr	result1, arg01
	and	result1, #1
_simplepin_getval_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
