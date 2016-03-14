DAT
	org	0

_foo
	mov	_foo__idx__0014, #5
L_032_
	add	OUTA, #1
	djnz	_foo__idx__0014, #L_032_
_foo_ret
	ret

_foo__idx__0014
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
	fit	496
