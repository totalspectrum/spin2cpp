PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test1
	mov	_test1_x, arg1 wz
	mov	_test1_y, arg2
	mov	_test1_z, arg3
 if_ne	jmp	#L__0002
	cmps	_test1_y, #0 wz
 if_ne	jmp	#L__0002
 if_e	jmp	#L__0001
L__0002
	mov	result1, _test1_z
	jmp	#_test1_ret
L__0001
	neg	result1, #1
_test1_ret
	ret

_test1_x
	long	0
_test1_y
	long	0
_test1_z
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
