pub start
    return @entry

dat

entry
	org
	mov	x, cnt
here
	jmp	#here
x
	long	$1
