DAT
	org	0

_foo
	mov	_foo__idx__0016, #5
L_047_
	add	OUTA, #1
	djnz	_foo__idx__0016, #L_047_
_foo_ret
	ret

_foo__idx__0016
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
