PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_ismagic
	cmps	arg1, #511 wz
 if_e	mov	_var_a, #1
 if_ne	mov	_var_a, #0
	mov	result1, _var_a
_ismagic_ret
	ret

_var_a
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
	fit	496
