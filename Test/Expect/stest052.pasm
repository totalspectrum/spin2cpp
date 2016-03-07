DAT
	org	0

_foo
	mov	result_, ptr_L_001__
_foo_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
ptr_L_001__
	long	@@@L_001_
result_
	long	0

L_001_
	byte	"hello"
	byte	0
