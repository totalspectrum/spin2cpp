DAT
	org	0
blah
	mov	bar_x_, #2
	call	#bar
blah_ret
	ret

bar
	add	bar_x_, #1
	mov	result_, bar_x_
bar_ret
	ret

bar_x_
	long	0
result_
	long	0
