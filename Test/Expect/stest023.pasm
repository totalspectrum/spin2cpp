DAT
	org	0
blah
	mov	result_, #3
blah_ret
	ret

bar
	add	arg1_, #1
	mov	result_, arg1_
bar_ret
	ret

arg1_
	long	0
result_
	long	0
