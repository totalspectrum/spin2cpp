DAT
	org	0

_foo
	mov	_foo__idx__0001, #5
L__0002
	add	OUTA, #1
	djnz	_foo__idx__0001, #L__0002
_foo_ret
	ret

_foo__idx__0001
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
