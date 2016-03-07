DAT
	org	0

test
	mov	DIRA, #1
	mov	OUTA, #1
test_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
