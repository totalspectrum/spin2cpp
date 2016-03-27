PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_count
	mov	_count_i, #0
L__0001
	mov	OUTA, _count_i
	add	_count_i, #1
	jmp	#L__0001
_count_ret
	ret

_count_i
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
	fit	496
