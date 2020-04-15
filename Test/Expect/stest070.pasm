pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_start
	rdlong	start_tmp002_, #0
	mov	_start__cse__0001, start_tmp002_
	shl	_start__cse__0001, #2
	add	_start__cse__0001, start_tmp002_
	mov	arg01, cnt
	add	arg01, _start__cse__0001
	waitcnt	arg01, #0
_start_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_start__cse__0001
	res	1
arg01
	res	1
start_tmp002_
	res	1
	fit	496
