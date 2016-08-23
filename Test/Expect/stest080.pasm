PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_blah
	and	arg1, #15
	mov	_tmp001_, OUTA
	andn	_tmp001_, #15
	or	_tmp001_, arg1
	mov	OUTA, _tmp001_
_blah_ret
	ret

_onepin
	test	arg1, #1 wz
	muxnz	OUTA, #16
_onepin_ret
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
