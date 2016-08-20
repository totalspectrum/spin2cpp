PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_foo
	mov	_var__idx__0001, #5
L__0002
	add	OUTA, #1
	djnz	_var__idx__0001, #L__0002
_foo_ret
	ret

_var__idx__0001
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
