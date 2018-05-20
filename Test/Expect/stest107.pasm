PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_dbl64
	add	arg2, arg2 wc
	addx	arg1, arg1
	mov	result1, arg1
	mov	result2, arg2
_dbl64_ret
	ret

_quad64
	call	#_dbl64
	mov	arg1, result1
	mov	arg2, result2
	call	#_dbl64
_quad64_ret
	ret

result1
	long	0
result2
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
