PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_clear0
	andn	OUTA, #1
_clear0_ret
	ret

_clearpin
	mov	clearpin_tmp001_, #1
	shl	clearpin_tmp001_, arg1
	andn	OUTA, clearpin_tmp001_
_clearpin_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
clearpin_tmp001_
	long	0
result1
	long	0
	fit	496
