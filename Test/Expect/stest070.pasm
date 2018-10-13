PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_start
	rdlong	start_tmp002_, #0
	mov	_start__cse__0023, start_tmp002_
	shl	_start__cse__0023, #2
	add	_start__cse__0023, start_tmp002_
	mov	arg1, CNT
	add	arg1, _start__cse__0023
	waitcnt	arg1, #0
_start_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_start__cse__0023
	res	1
arg1
	res	1
start_tmp002_
	res	1
	fit	496
