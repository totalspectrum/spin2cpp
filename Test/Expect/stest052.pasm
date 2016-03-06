DAT
	org	0

foo
	mov	result_, ptr_L_001__
foo_ret
	ret

ptr_L_001__
	long	@@@L_001_
result_
	long	0

L_001_
	byte	"hello"
	byte	0
