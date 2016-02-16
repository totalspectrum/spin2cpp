stest023_blah
	mov	stest023_bar_x_, #2
	call	#stest023_bar
stest023_blah_ret
	ret

stest023_bar
	add	stest023_bar_x_, #1
	mov	result_, stest023_bar_x_
stest023_bar_ret
	ret

result_
	long	0
stest023_bar_x_
	long	0
