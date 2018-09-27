PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_demo
	mov	_var_04, arg1
	mov	_var_04 + 1, arg2
	mov	_var_04 + 2, arg3
	mov	result1, _var_04 + 1
_demo_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
	res	1
_var_02
	res	1
_var_03
	res	1
_var_04
	res	3
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496
