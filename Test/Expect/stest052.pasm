DAT
	org	0

_foo
	mov	result1, ptr_L__0001_
_foo_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
ptr_L__0001_
	long	@@@L__0001
result1
	long	0
	fit	496

L__0001
	byte	"hello"
	byte	0
