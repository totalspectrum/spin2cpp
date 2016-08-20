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
	mov	_tmp001_, #1
	shl	_tmp001_, arg1
	andn	OUTA, _tmp001_
_clearpin_ret
	ret

_tmp001_
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
