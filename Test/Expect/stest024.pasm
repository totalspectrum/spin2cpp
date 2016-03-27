PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test1
	mov	_test1_x, arg1 wz
 if_e	jmp	#L__0001
	cmps	arg2, #0 wz
 if_ne	mov	result1, arg3
 if_ne	jmp	#_test1_ret
L__0001
	neg	result1, #1
_test1_ret
	ret

_test1_x
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
