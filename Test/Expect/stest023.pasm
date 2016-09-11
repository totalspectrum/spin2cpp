PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_blah
	mov	result1, #3
_blah_ret
	ret

_bar
	add	arg1, #1
	mov	result1, arg1
_bar_ret
	ret

result1
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
