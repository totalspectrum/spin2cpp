DAT
	org	0

testit
	test	arg1_, #4 wz
 if_ne	mov	result_, arg2_
 if_e	mov	result_, #0
testit_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
