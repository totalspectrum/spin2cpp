pub start
    return @entry

dat

entry
	org
	mov	x, cnt
	jmp	#$
x
	long	$1
