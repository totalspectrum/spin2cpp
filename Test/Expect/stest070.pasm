pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_start
	rdlong	arg01, #0
	mov	_start__cse__0000, arg01
	shl	_start__cse__0000, #2
	add	_start__cse__0000, arg01
	mov	arg01, cnt
	add	arg01, _start__cse__0000
	waitcnt	arg01, #0
_start_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_start__cse__0000
	res	1
arg01
	res	1
	fit	496
