pub start
    return

dat

entry
	org
	call	#myfunc
	jmp	$
myfunc
	add	x,#1
myfunc_ret
	ret
x	res 1


