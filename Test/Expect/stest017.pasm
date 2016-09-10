PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_count
	mov	_var_00, #0
L__0001
	mov	OUTA, _var_00
	add	_var_00, #1
	cmps	_var_00, #4 wz
 if_ne	jmp	#L__0001
_count_ret
	ret

_var_00
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
