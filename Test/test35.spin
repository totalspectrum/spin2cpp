pub start
    return

dat

entry
	org
	call	#myfunc
	jmp	$
myfunc
	add	x,#1
	add	y,#2
	add	z,#3
myfunc_ret
	ret

x	res 1
y	res 2
z	res 1
