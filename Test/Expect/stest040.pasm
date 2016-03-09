DAT
	org	0

_foo
	mov	_foo__idx__0006, #5
L_016_
	add	OUTA, #1
	djnz	_foo__idx__0006, #L_016_
_foo_ret
	ret

_foo__idx__0006
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
